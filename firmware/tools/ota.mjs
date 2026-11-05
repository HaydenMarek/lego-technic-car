#!/usr/bin/env node
// @ts-check
// Upload firmware to the car over Wi-Fi (OTA). Protocol: src/ota_logic.h.
//
// Usage (from the repo root):
//   node firmware/tools/ota.mjs [file.uf2] [--host lego-car.local]
// Defaults: firmware/build/pico2w/car_firmware.uf2, host from firmware/.env
// (OTA_HOST) or lego-car.local. The token comes from firmware/.env (OTA_TOKEN).

import dns from "node:dns/promises";
import { readFileSync } from "node:fs";
import net from "node:net";
import path from "node:path";
import { fileURLToPath } from "node:url";

const PORT = 4242;
const HEADER_SIZE = 64;
const BLOCK_SIZE = 512;
const CHUNK_SIZE = 4 * BLOCK_SIZE;
const FAMILY_RP2350_ARM_S = 0xe48bff59;
const UF2_MAGIC0 = 0x0a324655;
const UF2_FLAG_FAMILY = 0x2000;
const REPLY_TIMEOUT_MS = 10_000;

/** Replies from the Pico (src/ota_logic.c). */
const ERRORS = {
  EMAG: "bad header",
  ETOK: "wrong token (OTA_TOKEN in firmware/.env differs from the one built into the car)",
  EFAM: "not an RP2350 Arm image",
  ESIZ: "image too big for the slot",
  EUF2: "bad UF2 block",
  EORD: "block order",
  EADR: "block address outside the slot",
  EPAR: "no partition table on the Pico: flash it over USB first (firmware/README.md)",
  EFLS: "flash write failed",
  EBSY: "busy",
  EPRO: "protocol error",
};

const firmwareDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

/** @param {string} file @returns {Record<string, string>} */
function readEnv(file) {
  /** @type {Record<string, string>} */
  const env = {};
  let text;
  try {
    text = readFileSync(file, "utf8");
  } catch {
    throw new Error(`Missing ${file}. Copy firmware/.env.example to firmware/.env.`);
  }
  for (const line of text.split(/\r?\n/)) {
    const m = /^\s*([A-Z_]+)\s*=(.*)$/.exec(line);
    if (m) env[m[1]] = m[2].trim();
  }
  return env;
}

/** Keep only the RP2350 Arm blocks; drop the "absolute" E10 workaround block. */
function loadUf2(file) {
  const data = readFileSync(file);
  if (data.length === 0 || data.length % BLOCK_SIZE !== 0) {
    throw new Error(`${file}: not a UF2 file (size ${data.length})`);
  }
  /** @type {Buffer[]} */
  const blocks = [];
  for (let off = 0; off < data.length; off += BLOCK_SIZE) {
    const b = data.subarray(off, off + BLOCK_SIZE);
    if (b.readUInt32LE(0) !== UF2_MAGIC0) throw new Error(`${file}: bad UF2 block at ${off}`);
    const hasFamily = (b.readUInt32LE(8) & UF2_FLAG_FAMILY) !== 0;
    if (hasFamily && b.readUInt32LE(28) === FAMILY_RP2350_ARM_S) blocks.push(b);
  }
  if (blocks.length === 0) throw new Error(`${file}: no rp2350-arm-s blocks`);
  blocks.forEach((b, i) => {
    if (b.readUInt32LE(20) !== i || b.readUInt32LE(24) !== blocks.length) {
      throw new Error(`${file}: block numbering is not 0..${blocks.length - 1}`);
    }
  });
  return blocks;
}

/** @param {string} token @param {number} numBlocks */
function makeHeader(token, numBlocks) {
  const h = Buffer.alloc(HEADER_SIZE);
  h.write("LEGOOTA1", 0, "ascii");
  h.writeUInt32LE(numBlocks, 8);
  h.writeUInt32LE(FAMILY_RP2350_ARM_S, 12);
  h.write(token, 16, "utf8");
  return h;
}

/** Reads 4-byte replies from a socket, one at a time. */
function replyReader(/** @type {net.Socket} */ sock) {
  let buffered = Buffer.alloc(0);
  /** @type {((r: string) => void) | null} */
  let waiting = null;
  /** @type {Error | null} */
  let failed = null;
  /** @type {((e: Error) => void) | null} */
  let reject = null;

  const pump = () => {
    if (waiting && buffered.length >= 4) {
      const r = buffered.subarray(0, 4).toString("ascii");
      buffered = buffered.subarray(4);
      const w = waiting;
      waiting = reject = null;
      w(r);
    }
  };
  const fail = (/** @type {Error} */ e) => {
    failed ??= e;
    if (reject) {
      const r = reject;
      waiting = reject = null;
      r(failed);
    }
  };
  sock.on("data", (d) => {
    buffered = Buffer.concat([buffered, d]);
    pump();
  });
  sock.on("error", fail);
  sock.on("close", () => fail(new Error("the car closed the connection")));

  /** @returns {Promise<string>} */
  return () =>
    new Promise((resolve, rej) => {
      const timer = setTimeout(() => fail(new Error("no reply from the car (timeout)")), REPLY_TIMEOUT_MS);
      waiting = (r) => {
        clearTimeout(timer);
        resolve(r);
      };
      reject = (e) => {
        clearTimeout(timer);
        rej(e);
      };
      pump();
      if (waiting && failed && buffered.length < 4) fail(failed);
    });
}

/**
 * Look up the car's name. mDNS (*.local) answers can get lost: the Pico's
 * Wi-Fi power save wakes the radio only now and then (seen 2026-11-05:
 * first lookup failed, the next one worked). So try a few times.
 * @param {string} host
 */
async function resolveHost(host) {
  if (net.isIP(host)) return host;
  const TRIES = 5;
  for (let i = 1; ; i++) {
    try {
      return (await dns.lookup(host, { family: 4 })).address;
    } catch (e) {
      if (i >= TRIES) {
        throw new Error(`cannot find ${host} (${/** @type {Error} */ (e).message}). Is the car on Wi-Fi? Try --host <IP from serial>.`);
      }
      console.log(`${host} not found yet, retry ${i}/${TRIES - 1}...`);
      await new Promise((r) => setTimeout(r, 1000));
    }
  }
}

/** @param {string} reply @param {string} want */
function expect(reply, want) {
  if (reply === want) return;
  const why = ERRORS[/** @type {keyof typeof ERRORS} */ (reply)] ?? "unknown reply";
  throw new Error(`car replied ${reply}: ${why}`);
}

async function main() {
  const args = process.argv.slice(2);
  let host = "";
  let file = path.join(firmwareDir, "build", "pico2w", "car_firmware.uf2");
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--host") host = args[++i] ?? "";
    else if (args[i] === "-h" || args[i] === "--help") {
      console.log("node firmware/tools/ota.mjs [file.uf2] [--host lego-car.local]");
      return;
    } else file = args[i];
  }

  const env = readEnv(process.env.OTA_ENV_FILE || path.join(firmwareDir, ".env"));
  if (!env.OTA_TOKEN) throw new Error("OTA_TOKEN missing in firmware/.env");
  host ||= env.OTA_HOST || "lego-car.local";

  const blocks = loadUf2(file);
  const kb = Math.round((blocks.length * 256) / 1024);
  console.log(`${path.basename(file)}: ${blocks.length} blocks (${kb} KB) -> ${host}:${PORT}`);

  const address = await resolveHost(host);
  if (address !== host) console.log(`${host} is ${address}`);

  const sock = net.connect({ host: address, port: PORT });
  sock.setNoDelay(true);
  const nextReply = replyReader(sock);
  await new Promise((resolve, reject) => {
    sock.once("connect", resolve);
    sock.once("error", (e) =>
      reject(new Error(`cannot reach ${host}:${PORT} (${e.message}). Is the car on Wi-Fi? Try --host <IP from serial>.`)),
    );
  });

  sock.write(makeHeader(env.OTA_TOKEN, blocks.length));
  console.log("waiting for the car to stop...");
  expect(await nextReply(), "RDY!");

  const started = Date.now();
  const chunks = Math.ceil(blocks.length / 4);
  for (let c = 0; c < chunks; c++) {
    const chunk = Buffer.alloc(CHUNK_SIZE); // last one zero padded
    blocks.slice(c * 4, c * 4 + 4).forEach((b, i) => b.copy(chunk, i * BLOCK_SIZE));
    sock.write(chunk);
    const last = c === chunks - 1;
    expect(await nextReply(), last ? "DONE" : "ACK!");
    if (c % 25 === 0 || last) {
      process.stdout.write(`\r${Math.round(((c + 1) / chunks) * 100)} %`);
    }
  }
  const secs = ((Date.now() - started) / 1000).toFixed(1);
  console.log(`\nuploaded in ${secs} s. The car reboots into the new firmware.`);
  console.log("If the new firmware fails to join Wi-Fi within 15 s, the car goes back to the old one.");
  sock.end();
}

main().catch((e) => {
  console.error(`OTA failed: ${e.message}`);
  process.exit(1);
});
