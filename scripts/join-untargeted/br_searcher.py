#! /usr/bin/env python3

import os
import sys


ROOT_PATH = "/home/neil/coreutils-8.30"
file_list = [l.strip() for l in open(sys.argv[1])]
gdb_batch_cmds = ['set logging on', 'set verbose off']
branch_kw = {'if', 'for', 'while', 'switch'}

for fp in file_list:
  with open(os.path.join(ROOT_PATH, fp)) as f:
    for linenum, line in enumerate(f, start=1):
      if any((kw in line for kw in branch_kw)):
        gdb_batch_cmds.append('b {}:{}'.format(fp, linenum))

with open('batch.gdb', 'w') as f:
  f.write('\n'.join(gdb_batch_cmds))

for b in sys.argv[2:]:
  os.system('rm -f gdb.txt')
  os.system('gdb --batch --command=batch.gdb --args ./{}'.format(b))
  os.system('grep -o -E "0x[0-9a-f]+" gdb.txt > {}.br'.format(b))

