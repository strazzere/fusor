#! /usr/bin/env python3

import csv
import sys
import os


if __name__ == '__main__':
  import argparse

  parser = argparse.ArgumentParser()
  parser.add_argument("llvm", type=str, default='func.csv', help='LLVM results path')
  parser.add_argument("concrete", type=str, default='funccount.csv', help='Pintool results path')
  args = parser.parse_args()

  # load LLVM
  with open(args.llvm) as f:
    llvm_functions_touched = set(map(str.strip, f))

  # load pin
  with open(args.concrete, newline='') as f:
    pin_results = list(map(lambda x: list(map(str.strip, x.split(','))), f))
  pin_map = {e[0]: e[3] for e in pin_results if e[0] in llvm_functions_touched}
