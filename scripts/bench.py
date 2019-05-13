#! /usr/bin/env python3

import argparse
import claripy
import angr
import time
import csv
import sys
import os

from IPython import embed
from collections import defaultdict
from functools import reduce

def load_csv(filepath):
  with open('./funccount.csv', newline='') as csvfile:
    csvreader = csv.reader(csvfile, delimiter=',', quotechar='"')
    lines = list(csvreader)

  return {l[0] for l in lines}


def find_parent_node(addr, cfg):
  addrs = sorted([(n.addr, addr - n.addr) for n in cfg.nodes() if addr >= n.addr], key=lambda x: x[1])
  return cfg.get_any_node(addrs[0][0])



class CFGHelper:
  def __init__(self, cfg):
    self.cfg = cfg
    self.functions = cfg.functions

  def get_function_name(self, addr, anyaddr=True, strict=False):
    if strict:
      return getattr(self.cfg.functions.get(addr), 'name', None)
    else:
      node = self.cfg.get_any_node(addr, anyaddr=anyaddr)
      if node:
        return getattr(self.cfg.functions.get(node.function_address), 'name', None)
      else:
        return None

  def generate_function_call_seq(self, state, target_functions):
    return list(filter(lambda x: x in target_functions, [self.functions.get(s) for s in state.history.bbl_addrs]))

  def get_callstack(self, stack):
    return [self.get_function_name(s.func_addr) for s in stack]


class BreakP:
  def __init__(self, cfg, helper):
    self.func_map = defaultdict(set)
    self.symbolic_input = set()
    self.symbolic_variables = set()
    self.expres = []
    self.explored_functions = []
    self.constraints = []
    self.regs = []
    self.mems = []
    self.helper = helper
    self.cfg = cfg

  def expr_bp(self, state):
    if state.inspect.expr_result.symbolic:
      sym_in = {s for s in state.inspect.expr_result.variables if self._symbolic_input_identifier(s)}
      if len(sym_in) > 0:
        func_name = self.helper.get_function_name(state.addr)
        self.func_map[func_name].update(sym_in)
        self.expres.append(state.inspect.expr_result)

  def sv_bp(self, state):
    self.symbolic_variables.add(state.inspect.symbolic_name)
    sname = state.inspect.symbolic_name
    if self._symbolic_input_identifier(sname):
      self.symbolic_input.add((state.addr, sname, tuple(filter(lambda x: x, [self.helper.get_function_name(bbl, strict=True) for bbl in state.history.bbl_addrs]))))
      print('New symbolic input')

  def func_bp(self, state):
    self.explored_functions.append((state.addr, state.inspect.function_address))
    # if self.r2.get_func(state.addr)[1] == 'test':
      # embed()

  def reg_read(self, state):
    reg_expr = state.inspect.reg_read_expr
    if reg_expr.symbolic:
      self.regs.append((state.addr, self.helper.get_function_name(state.addr), reg_expr))

  def mem_read(self, state):
    mem_expr = state.inspect.mem_read_expr
    if mem_expr.symbolic:
      self.mems.append((state.addr, self.helper.get_function_name(state.addr), mem_expr))
  
  def constraint_bp(self, state):
    added_constraints = state.inspect.added_constraints
    if not all(map(lambda x: x.concrete, added_constraints)):
      self.constraints.append((state.addr, self.helper.get_function_name(state.addr), added_constraints))

  def _symbolic_input_identifier(self, name):
    return name.startswith('packet') or name.startswith('file')

  def contains_symbolic_input(self, constraints):
    si_name = {s[1] for s in self.symbolic_input}
    return any((len(si_name.intersection(c.variables)) > 0 for c in constraints))

  def paths_gen(self):
    path_grp = set()
    for a in simgr.active:
      svars = reduce(lambda x, y: x.union(y), map(lambda x: x.variables, a.solver.constraints))
      path_grp.add((a, svars, self.helper.generate_function_call_seq(a, self.func_map)))


class atexit(angr.SimProcedure):
  def run(self, argc, argv):
    print('\n\nSkip atexit\n\n')


if __name__ == '__main__':
  import signal
  def kill():
    os.system('kill %d' % os.getpid())

  def sigint_handler(signum, frame):
    print('Stopping Execution for Debug. If you want to kill the programm issue: killmyself()')
    embed()

  signal.signal(signal.SIGINT, sigint_handler)

  parser = argparse.ArgumentParser()
  parser.add_argument("-a", "--disable_auto_load", help="Show generated model", action="store_true")
  parser.add_argument("-v", "--verbose", help="Show debug info", action="store_true")
  parser.add_argument("-d", "--dfs", help="Use DFS", action="store_true")
  parser.add_argument("-c", "--csv", default="funccount.csv", type=str, help="csvfile path")
  parser.add_argument("-i", "--inspect", help="disable bp", action="store_false")
  parser.add_argument("-C", "--coverage", help="Collect branch coverage", action="store_true")
  parser.add_argument("file_path", type=str, help="Binary path")
  args = parser.parse_args()
  
  p = angr.Project(args.file_path, auto_load_libs= not args.disable_auto_load)
  if args.verbose:
    angr.manager.l.setLevel('DEBUG')
  
  cfg = p.analyses.CFGFast(normalize=True)
  brs = {s for s in cfg.nodes() if len(s.successors) > 1}
  edges = {(b.addr, s.addr) for b in brs for s in b.successors}

  target_functions = load_csv(args.csv)
  helper = CFGHelper(cfg)
  sv = claripy.BVS('packet_args', 8 * 24)
  s = p.factory.entry_state(args=[p.filename, sv])
  bp = BreakP(cfg, helper)
  
  if args.inspect:
    s.inspect.b('call', when=angr.BP_AFTER, action=bp.func_bp)
    s.inspect.b('symbolic_variable', when=angr.BP_AFTER, action=bp.sv_bp)
    s.inspect.b('expr', when=angr.BP_AFTER, action=bp.expr_bp)
    # s.inspect.b('constraints', when=angr.BP_AFTER, action=bp.constraint_bp)
    # s.inspect.b('reg_read', when=angr.BP_AFTER, action=bp.reg_read)
    s.inspect.b('mem_read', when=angr.BP_AFTER, action=bp.mem_read)
    
  # p.hook_symbol('atexit', atexit())
  # p.hook_symbol('set_program_name', atexit())
  # p.hook_symbol('getopt_long', atexit())
  
  simgr = p.factory.simgr(s)
  
  # simgr.explore(find=0x403610)
  # embed()
  
  def tracer(simgr, edges, start, p):
    global cnt
    write = False
    for a in simgr.active:
      history_bbs = list(a.history.bbl_addrs)
      for b1, b2 in zip(history_bbs, history_bbs[1:]):
        if (b1, b2) in edges:
          cnt += 1
          write = True
          edges.remove((b1, b2))
      if write:
        with open('{}.csv'.format(p.filename), 'a') as f:
          f.write('{}, {}, {:.2f}\n'.format(len(edges), cnt, time.time() - start))
        write = False

  if args.coverage:
    if os.path.exists('{}.csv'.format(p.filename)):
      os.remove('{}.csv'.format(p.filename))

  if args.dfs:
    simgr.use_technique(angr.exploration_techniques.DFS())
    simgr.run(until=lambda x: len(simgr.deadended) > 3000)
  else:
    start = time.time()
    cnt = 0
    # simgr.run(until=lambda x: time.time() - start > 1800, callback=lambda: tracer(simgr, edges, start, p))
    simgr.explore(find=0x416c92, until=lambda x: time.time() - start > 2 * 3600)
    end = time.time()
    print(end - start)
    # simgr.run(until=lambda x: len(simgr.active) > 1000)
    # simgr.explore()
  
  # cfg = p.analyses.CFGFast(normalize=True)
  # fs = {cfg.functions[f[1].args[0]].name for f in functions}
  # functions = dict(p.kb.functions)
  
  embed()
  
  # cfg = p.analyses.CFGFast(normalize=True)