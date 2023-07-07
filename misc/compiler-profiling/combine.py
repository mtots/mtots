
#!/usr/bin/env python3
"""Combine JSON from multiple -ftime-traces into one.
Run with (e.g.): python combine_traces.py foo.json bar.json."""
import json
import sys
from typing import List, Any
if __name__ == '__main__':
    timeMap = {}
    start_time = 0
    combined_data: List[Any] = []
    for filename in sys.argv[1:]:
        with open(filename, 'r') as f:
            # print(f"FILE {filename}")
            file_time = None
            for event in json.load(f)['traceEvents']:
                if event['name'] == 'ExecuteCompiler':
                    if file_time is not None:
                        raise Exception("file_time set twice")
                    # Find how long this compilation takes
                    file_time = event['dur']
                    timeMap[filename] = file_time
                    # print(f"file_time -> {file_time}")
                    # Set the file name in ExecuteCompiler
                    print(f"{filename} -> {event}")
                    if 'args' not in event:
                        event['args'] = {}
                    # event['args']['detail'] = filename
                    event['tid'] = 'total'
                event['pid'] = filename

                # # Skip metadata events
                # # Skip total events
                # # Filter out shorter events to reduce data size
                # if event['ph'] == 'M' or \
                #         event['name'].startswith('Total') or \
                #         event['dur'] < 5000:
                #     # print(f"  SKIPPING EVENT {event}")
                #     continue

                # Offset start time to make compiles sequential
                event['ts'] += start_time
                # Add data to combined
                combined_data.append(event)
            # Increase the start time for the next file
            # Add 1 to avoid issues with simultaneous events
            # start_time += file_time + 1
    with open('out/summary.json', 'w') as f:
        json.dump({'traceEvents': sorted(combined_data, key=lambda k: k['ts'])}, f)

    for name, dur in reversed(sorted(timeMap.items(), key=lambda p: p[1])):
        print(f"{name} -> {dur / 1000000}s")
    print(f"total {sum(timeMap.values()) / 1000000}s")
