# PA2 - Reliable File Transfer

Grading script for the Reliable File Transfer assignment (PA2) in CSCI 4273/5273.

## Setup

Place `grade.py` in the same directory as `udp_server.c`, `udp_client.c`, `foo1`, `foo2`, `foo3`, and `Makefile`.

```
<your-project-root>/
├── Makefile
├── udp_server.c
├── udp_client.c
├── foo1
├── foo2
├── foo3
└── grade.py
```

The Makefile should build `server/udp_server` and `client/udp_client`, creating those directories if needed, and copy `foo1`, `foo2`, `foo3` into `server/`.

## Usage

```
python3 grade.py \
  --host 127.0.0.1 \
  --port 8000 \
  --timeout 3.0 \
  --wait 60.0 \
  --foo2-size-mb 2 \
  --loss 5.0 \
  --delay 1ms
```

Adjust `--wait` based on your implementation (go-back-N or stop-and-wait) and file size.

Note: the netem test requires Linux (`tc` command). Running on other OS will skip packet loss/latency tests.

## Tests

| # | Test | Points |
|---|------|--------|
| 1 | `ls` lists foo1, foo2, foo3 | 15 |
| 2 | `get foo1` - hash matches server copy | 15 |
| 3 | `delete foo1` then `ls` - foo1 not listed | 15 |
| 4 | `put foo1` - hash matches client copy | 15 |
| 5 | `exit` - server exits cleanly | 15 |
| 6 | `get foo2` under netem delay + loss - hash matches | 25 |

**Total: 100 points**

## netem cleanup

If the script is interrupted, clear the qdisc manually:
```
sudo tc qdisc del dev lo root
```
