import json
import io
import sys
import string

proc log_error(msg):
    print "::error::" + msg

proc validate_json(path):
    let content = io.readfile(path)
    if content == nil:
        log_error("Could not read file: " + path)
        return nil
    let cjson = json.cJSON_Parse(content)
    if cjson == nil:
        log_error("Failed to parse JSON in: " + path)
        return nil
    let data = json.cJSON_ToSage(cjson)
    json.cJSON_Delete(cjson)
    return data

proc file_exists(path):
    # sys.exec returns 0 on success; non-zero if the file is missing or not a regular file
    return sys.exec("test -f '" + path + "'") == 0

proc main():
    print "Starting validation..."

    let index = validate_json("packages.json")
    if index == nil:
        sys.exit(1)

    let pkgs = index["packages"]
    if pkgs == nil:
        log_error("packages.json is missing 'packages' array")
        sys.exit(1)

    let success = true

    for i in range(len(pkgs)):
        let p = pkgs[i]
        let name = p["name"]
        let version = p["version"]

        print "Validating package: " + name + " (v" + version + ")"

        let meta_path = "packages/" + name + "/metadata.json"
        let meta = validate_json(meta_path)
        if meta == nil:
            success = false
            continue

        if meta["name"] != name:
            log_error("Name mismatch in " + meta_path + ": expected '" + name + "', got '" + meta["name"] + "'")
            success = false

        if meta["version"] != version:
            log_error("Version mismatch in " + meta_path + ": expected '" + version + "', got '" + meta["version"] + "'")
            success = false

        # Check every file declared in metadata physically exists in the repo
        let files = meta["files"]
        if files != nil:
            for j in range(len(files)):
                let f = files[j]
                let f_path = "packages/" + name + "/" + f
                if not file_exists(f_path):
                    log_error("Declared file missing from repo: " + f_path)
                    success = false

    if not success:
        print "Validation failed."
        sys.exit(1)

    print "Validation successful!"

main()
