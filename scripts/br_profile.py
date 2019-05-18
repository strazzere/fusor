#! /usr/bin/env python3

import argparse
import claripy
import angr
import time
import csv
import sys
import os
import json


if len(sys.argv) == 2:
  p = angr.Project(sys.argv[1], auto_load_libs=True)
  cfg = p.analyses.CFGFast(normalize=True)
  nodes = [(n.addr, getattr(cfg.functions.get(n.function_address), 'name', 'null')) for n in cfg.nodes() if len(n.successors) > 1]
  with open('load.txt', 'w') as f:
    for n in nodes:
      f.write('{}\n'.format(hex(n[0])))
  with open('brinfo.json','w') as f:
    json.dump(nodes, f)

else:
  with open('brinfo.json') as f:
    fmap = {i: j for i, j in json.load(f)}

  with open(sys.argv[1]) as f:
    icnt = {int(l.split(',')[1], base=16): int(l.split(',')[0]) for l in f}
  
  from collections import defaultdict
  result = defaultdict(lambda : 0)
  for i, cnt in icnt.items():
    result[fmap[i]] += cnt

  with open(sys.argv[1].split('.')[0] + '.final.csv', 'w') as f:
    for fn, cnt in result.items():
      f.write('{}, {}\n'.format(fn, cnt))

