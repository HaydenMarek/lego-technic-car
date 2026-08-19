"""Load selected pure definitions from a Hub entry point without Pybricks."""

import ast


def load_definitions(path, *names):
    """Return selected top-level assignments, functions, and classes."""

    requested = set(names)
    selected = []
    found = set()
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))

    for node in tree.body:
        name = None
        if isinstance(node, (ast.FunctionDef, ast.ClassDef)):
            name = node.name
        elif isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
            if isinstance(target, ast.Name):
                name = target.id

        if name in requested:
            selected.append(node)
            found.add(name)

    missing = requested - found
    if missing:
        raise RuntimeError("missing Hub definitions: {0}".format(sorted(missing)))

    namespace = {}
    module = ast.Module(body=selected, type_ignores=[])
    exec(compile(module, str(path), "exec"), namespace)
    return {name: namespace[name] for name in requested}
