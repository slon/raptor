#!/usr/bin/python
import os
import sys
import re
import datetime

def looks_like_log_file(filename):
    for lvl in ["INFO", "WARNING", "ERROR", "FATAL"]:
        if lvl in filename:
            return True
    return False

if __name__ == "__main__":
    log_dir = sys.argv[1]
    os.chdir(log_dir)

    all_files = os.listdir(log_dir)

    symlinks = filter(os.path.islink, all_files)

    active_files = map(os.readlink, symlinks)

    files_ready_for_rotation = list(set(all_files) - set(symlinks) - set(active_files))

    today = datetime.datetime.today()
    for log_file in files_ready_for_rotation:
        if not looks_like_log_file(log_file):
            continue

        st = os.stat(log_file)
        last_access_time = datetime.datetime.fromtimestamp(os.stat(log_file).st_mtime)

        print log_file, today, last_access_time

        if (today - last_access_time).days > 7:
            os.unlink(log_file)
