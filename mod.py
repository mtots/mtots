import os
import re

uniq = {"": ""}
uniq.pop("")


def modify(contents: str) -> str:
    def toUpperAndRemoveUnderscores(match: re.Match[str]) -> str:
        return match.group(0).upper().replace("_", "")

    def capitalize(s: str) -> str:
        return s[0].upper() + s[1:]

    while True:
        match = re.search(r"\b(?!MTOTS_)\w+_VAL\b", contents)
        if not match:
            break
        original = match.group(0)
        repl = (
            "valCFunction"
            if original == "CFUNCTION_VAL"
            else "valFRect"
            if original == "FRECT_VAL"
            else "val"
            + capitalize(
                re.sub(
                    r"_\w",
                    toUpperAndRemoveUnderscores,
                    original.removesuffix("_VAL").lower(),
                )
            )
        )
        uniq[original] = repl
        contents = re.sub(r"\b" + original + r"\b", repl, contents)
    return contents


for sourceName in os.listdir("src"):
    path = os.path.join("src", sourceName)
    with open(path, "r") as f:
        contents = f.read()
    modified = modify(contents)
    with open(path, "w") as f:
        f.write(modified)

for key, value in uniq.items():
    print(f"{key} -> {value}")
