#! /usr/bin/env python3

import fileinput

sums = 0
for l in fileinput.input():
  sums += int(l)

print(sums)
