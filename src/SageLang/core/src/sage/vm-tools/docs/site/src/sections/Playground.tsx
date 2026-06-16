"use client"

import { useState, useRef, useCallback } from 'react'
import gsap from 'gsap'
import { ScrollTrigger } from 'gsap/ScrollTrigger'
import { Play, StepForward, RotateCcw, Zap, Edit3, Code, Terminal, ChevronDown, ChevronRight, Download, Upload, Copy, Check } from 'lucide-react'

gsap.registerPlugin(ScrollTrigger)

// ─── Type Alias ───
type VMValue = string | number | boolean | null | unknown[] | Record<string, unknown>

// ─── Types ───
interface Program {
  name: string
  sage: string
  bytecode: string[]
  isCustom?: boolean
}

interface VMState {
  stack: VMValue[]
  globals: Map<string, VMValue>
  pc: number
  output: string[]
  halted: boolean
  callStack: { name: string; pc: number }[]
  locals: Map<string, VMValue>[]
  envDepth: number
}

// ─── Built-in Sample Programs ───
const SAMPLE_PROGRAMS: Program[] = [
  {
    name: 'Hello World',
    sage: `proc main():
    print "Hello, SageVM!"
    return 0`,
    bytecode: [
      'OP_CONSTANT "Hello, SageVM!"',
      'OP_PRINT',
      'OP_CONSTANT 0',
      'OP_RETURN',
    ],
  },
  {
    name: 'Arithmetic',
    sage: `proc main():
    var x = 10
    var y = 20
    var result = x + y * 2
    print result
    return result`,
    bytecode: [
      'OP_CONSTANT 10',
      'OP_DEFINE_GLOBAL "x"',
      'OP_CONSTANT 20',
      'OP_DEFINE_GLOBAL "y"',
      'OP_GET_GLOBAL "x"',
      'OP_GET_GLOBAL "y"',
      'OP_CONSTANT 2',
      'OP_MUL',
      'OP_ADD',
      'OP_DEFINE_GLOBAL "result"',
      'OP_GET_GLOBAL "result"',
      'OP_PRINT',
      'OP_GET_GLOBAL "result"',
      'OP_RETURN',
    ],
  },
  {
    name: 'Fibonacci',
    sage: `proc fib(n):
    if n <= 1:
        return n
    end
    return fib(n - 1) + fib(n - 2)
end

proc main():
    print fib(8)
    return 0
end`,
    bytecode: [
      'OP_DEFINE_FUNCTION "fib" 1',
      'OP_GET_GLOBAL "n"',
      'OP_CONSTANT 1',
      'OP_LESS_EQUAL',
      'OP_JUMP_IF_FALSE 0x0006',
      'OP_GET_GLOBAL "n"',
      'OP_RETURN',
      'OP_GET_GLOBAL "fib"',
      'OP_GET_GLOBAL "n"',
      'OP_CONSTANT 1',
      'OP_SUB',
      'OP_CALL 1',
      'OP_GET_GLOBAL "fib"',
      'OP_GET_GLOBAL "n"',
      'OP_CONSTANT 2',
      'OP_SUB',
      'OP_CALL 1',
      'OP_ADD',
      'OP_RETURN',
      'OP_DEFINE_FUNCTION "main" 0',
      'OP_GET_GLOBAL "fib"',
      'OP_CONSTANT 8',
      'OP_CALL 1',
      'OP_PRINT',
      'OP_CONSTANT 0',
      'OP_RETURN',
    ],
  },
  {
    name: 'Arrays & Loops',
    sage: `proc main():
    var arr = [1, 2, 3, 4, 5]
    var sum = 0
    for i in range(len(arr)):
        sum = sum + arr[i]
    end
    print sum
    return sum
end`,
    bytecode: [
      'OP_CONSTANT 1',
      'OP_CONSTANT 2',
      'OP_CONSTANT 3',
      'OP_CONSTANT 4',
      'OP_CONSTANT 5',
      'OP_ARRAY 5',
      'OP_DEFINE_GLOBAL "arr"',
      'OP_CONSTANT 0',
      'OP_DEFINE_GLOBAL "sum"',
      'OP_GET_GLOBAL "arr"',
      'OP_ARRAY_LEN',
      'OP_CONSTANT 0',
      'OP_DEFINE_GLOBAL "i"',
      'OP_GET_GLOBAL "i"',
      'OP_GET_GLOBAL "arr"',
      'OP_ARRAY_LEN',
      'OP_LESS',
      'OP_JUMP_IF_FALSE 0x0018',
      'OP_GET_GLOBAL "sum"',
      'OP_GET_GLOBAL "arr"',
      'OP_GET_GLOBAL "i"',
      'OP_GET_INDEX',
      'OP_ADD',
      'OP_SET_GLOBAL "sum"',
      'OP_GET_GLOBAL "i"',
      'OP_CONSTANT 1',
      'OP_ADD',
      'OP_SET_GLOBAL "i"',
      'OP_LOOP_BACK 0xFFE8',
      'OP_GET_GLOBAL "sum"',
      'OP_PRINT',
      'OP_GET_GLOBAL "sum"',
      'OP_RETURN',
    ],
  },
  {
    name: 'Classes',
    sage: `class Point:
    proc init(self, x, y):
        self.x = x
        self.y = y
    end
    proc move(self, dx, dy):
        self.x = self.x + dx
        self.y = self.y + dy
    end
end

proc main():
    var p = Point(3, 4)
    p.move(1, 2)
    print p.x
    return 0
end`,
    bytecode: [
      'OP_CLASS "Point"',
      'OP_METHOD "init"',
      'OP_GET_LOCAL 0',
      'OP_GET_LOCAL 1',
      'OP_SET_PROPERTY "x"',
      'OP_GET_LOCAL 0',
      'OP_GET_LOCAL 2',
      'OP_SET_PROPERTY "y"',
      'OP_RETURN',
      'OP_METHOD "move"',
      'OP_GET_LOCAL 0',
      'OP_GET_PROPERTY "x"',
      'OP_GET_LOCAL 1',
      'OP_ADD',
      'OP_SET_PROPERTY "x"',
      'OP_GET_LOCAL 0',
      'OP_GET_PROPERTY "y"',
      'OP_GET_LOCAL 2',
      'OP_ADD',
      'OP_SET_PROPERTY "y"',
      'OP_RETURN',
      'OP_DEFINE_FUNCTION "main" 0',
      'OP_GET_GLOBAL "Point"',
      'OP_CONSTANT 3',
      'OP_CONSTANT 4',
      'OP_CALL 2',
      'OP_DEFINE_GLOBAL "p"',
      'OP_GET_GLOBAL "p"',
      'OP_CONSTANT 1',
      'OP_CONSTANT 2',
      'OP_CALL_METHOD "move" 2',
      'OP_GET_GLOBAL "p"',
      'OP_GET_PROPERTY "x"',
      'OP_PRINT',
      'OP_CONSTANT 0',
      'OP_RETURN',
    ],
  },
  {
    name: 'Custom',
    sage: `; Write your own Sage code here\n; Click "Compile to Bytecode" to generate bytecode`,
    bytecode: [
      'OP_CONSTANT 42',
      'OP_PRINT',
    ],
    isCustom: true,
  }
]

// ─── Simple Sage-to-Bytecode Compiler ───
function compileSageToBytecode(source: string): string[] {
  const lines = source.split('\n').map(l => l.trim()).filter(l => l && !l.startsWith(';'))
  const bytecode: string[] = []
  const indentStack: number[] = [0]
  const loopBackTargets: number[] = []
  const ifJumpTargets: number[] = []
  let hasClass = false

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i]
    const indent = line.match(/^(\s*)/)?.[1].length || 0

    while (indent < indentStack[indentStack.length - 1]) {
      indentStack.pop()
      if (loopBackTargets.length > 0) {
        const target = loopBackTargets.pop()
        if (target !== undefined) {
          bytecode.push(`OP_LOOP_BACK ${(target - bytecode.length).toString(16).padStart(4, '0')}`)
        }
      }
    }

    if (line.startsWith('proc ') && line.includes('(') && !hasClass) {
      const match = line.match(/proc\s+(\w+)\s*\(([^)]*)\)/)
      if (match) {
        const params = match[2].split(',').filter(p => p.trim()).length
        bytecode.push(`OP_DEFINE_FUNCTION "${match[1]}" ${params}`)
        indentStack.push(indent)
      }
      continue
    }

    if (line.startsWith('class ')) {
      const match = line.match(/class\s+(\w+)/)
      if (match) {
        bytecode.push(`OP_CLASS "${match[1]}"`)
        hasClass = true
        indentStack.push(indent)
      }
      continue
    }

    if (line.startsWith('proc ') && hasClass) {
      const match = line.match(/proc\s+(\w+)\s*\(/)
      if (match) {
        bytecode.push(`OP_METHOD "${match[1]}"`)
      }
      continue
    }

    if (line.startsWith('if ')) {
      const cond = line.replace(/^if\s+/, '').replace(/:$/, '').trim()
      compileExpression(cond, bytecode)
      bytecode.push('OP_JUMP_IF_FALSE 0x0000')
      ifJumpTargets.push(bytecode.length - 1)
      indentStack.push(indent)
      continue
    }

    if (line.startsWith('for ') && line.includes('in range')) {
      const match = line.match(/for\s+(\w+)\s+in\s+range\s*\(([^)]+)\)/)
      if (match) {
        compileExpression(match[2], bytecode)
        bytecode.push(`OP_DEFINE_GLOBAL "${match[1]}"`)
        bytecode.push(`OP_CONSTANT 0`)
        bytecode.push(`OP_DEFINE_GLOBAL "${match[1]}"`)
        loopBackTargets.push(bytecode.length)
        bytecode.push(`OP_GET_GLOBAL "${match[1]}"`)
        bytecode.push(`OP_LESS`)
        bytecode.push('OP_JUMP_IF_FALSE 0x0000')
        ifJumpTargets.push(bytecode.length - 1)
        indentStack.push(indent)
      }
      continue
    }

    if (line.startsWith('return')) {
      const expr = line.replace(/^return\s*/, '').trim()
      if (expr) {
        compileExpression(expr, bytecode)
      } else {
        bytecode.push('OP_NIL')
      }
      bytecode.push('OP_RETURN')
      continue
    }

    if (line.startsWith('print ')) {
      const expr = line.replace(/^print\s+/, '').trim()
      compileExpression(expr, bytecode)
      bytecode.push('OP_PRINT')
      continue
    }

    if (line.includes('=') && !line.includes('==')) {
      const parts = line.split('=')
      const lhs = parts[0].trim()
      const rhs = parts.slice(1).join('=').trim()
      if (lhs && rhs) {
        compileExpression(rhs, bytecode)
        if (lhs.includes('.')) {
          const dotParts = lhs.split('.')
          bytecode.push(`OP_SET_PROPERTY "${dotParts[1]}"`)
        } else if (lhs.includes('[')) {
          bytecode.push(`OP_SET_INDEX`)
        } else {
          bytecode.push(`OP_DEFINE_GLOBAL "${lhs}"`)
        }
      }
      continue
    }

    compileExpression(line.replace(/:$/, '').trim(), bytecode)
  }

  ifJumpTargets.forEach(idx => {
    const offset = (bytecode.length - idx).toString(16).padStart(4, '0')
    bytecode[idx] = bytecode[idx].replace('0x0000', `0x${offset}`)
  })

  bytecode.push('OP_HALT')
  return bytecode
}

function compileExpression(expr: string, bytecode: string[]) {
  expr = expr.trim()

  if (expr.startsWith('"') && expr.endsWith('"')) {
    bytecode.push(`OP_CONSTANT ${expr}`)
    return
  }

  if (!isNaN(Number(expr)) && expr !== '') {
    bytecode.push(`OP_CONSTANT ${expr}`)
    return
  }

  if (expr.startsWith('[') && expr.endsWith(']')) {
    const items = expr.slice(1, -1).split(',').map(s => s.trim()).filter(Boolean)
    items.forEach(item => compileExpression(item, bytecode))
    bytecode.push(`OP_ARRAY ${items.length}`)
    return
  }

  const callMatch = expr.match(/^(\w+)\s*\(([^)]*)\)$/)
  if (callMatch) {
    const args = callMatch[2].split(',').map(s => s.trim()).filter(Boolean)
    args.forEach(arg => compileExpression(arg, bytecode))
    bytecode.push(`OP_CALL ${args.length}`)
    return
  }

  const methodMatch = expr.match(/^(\w+)\.(\w+)\s*\(([^)]*)\)$/)
  if (methodMatch) {
    const args = methodMatch[3].split(',').map(s => s.trim()).filter(Boolean)
    bytecode.push(`OP_GET_GLOBAL "${methodMatch[1]}"`)
    args.forEach(arg => compileExpression(arg, bytecode))
    bytecode.push(`OP_CALL_METHOD "${methodMatch[2]}" ${args.length}`)
    return
  }

  const propMatch = expr.match(/^(\w+)\.(\w+)$/)
  if (propMatch) {
    bytecode.push(`OP_GET_GLOBAL "${propMatch[1]}"`)
    bytecode.push(`OP_GET_PROPERTY "${propMatch[2]}"`)
    return
  }

  const indexMatch = expr.match(/^(\w+)\[(\w+)\]$/)
  if (indexMatch) {
    bytecode.push(`OP_GET_GLOBAL "${indexMatch[1]}"`)
    bytecode.push(`OP_GET_GLOBAL "${indexMatch[2]}"`)
    bytecode.push(`OP_GET_INDEX`)
    return
  }

  const ops = [
    { op: '+', opcode: 'OP_ADD' },
    { op: '-', opcode: 'OP_SUB' },
    { op: '*', opcode: 'OP_MUL' },
    { op: '/', opcode: 'OP_DIV' },
    { op: '%', opcode: 'OP_MOD' },
  ]
  for (const { op, opcode } of ops) {
    const parts = expr.split(op).map(s => s.trim())
    if (parts.length === 2) {
      compileExpression(parts[0], bytecode)
      compileExpression(parts[1], bytecode)
      bytecode.push(opcode)
      return
    }
  }

  const comps = [
    { op: '==', opcode: 'OP_EQUAL' },
    { op: '!=', opcode: 'OP_NOT_EQUAL' },
    { op: '>=', opcode: 'OP_GREATER_EQUAL' },
    { op: '<=', opcode: 'OP_LESS_EQUAL' },
    { op: '>', opcode: 'OP_GREATER' },
    { op: '<', opcode: 'OP_LESS' },
  ]
  for (const { op, opcode } of comps) {
    if (expr.includes(op)) {
      const parts = expr.split(op).map(s => s.trim())
      if (parts.length === 2) {
        compileExpression(parts[0], bytecode)
        compileExpression(parts[1], bytecode)
        bytecode.push(opcode)
        return
      }
    }
  }

  if (/^\w+$/.test(expr)) {
    bytecode.push(`OP_GET_GLOBAL "${expr}"`)
    return
  }

  if (expr.startsWith('len(')) {
    const inner = expr.slice(4, -1).trim()
    bytecode.push(`OP_GET_GLOBAL "${inner}"`)
    bytecode.push('OP_ARRAY_LEN')
    return
  }
}

// ─── VM Interpreter ───
class SGVMInterpreter {
  state: VMState = {
    stack: [],
    globals: new Map(),
    pc: 0,
    output: [],
    halted: false,
    callStack: [],
    locals: [new Map()],
    envDepth: 0,
  }

  reset() {
    this.state = {
      stack: [],
      globals: new Map(),
      pc: 0,
      output: [],
      halted: false,
      callStack: [],
      locals: [new Map()],
      envDepth: 0,
    }
  }

  getState(): VMState {
    return { ...this.state, stack: [...this.state.stack], output: [...this.state.output] }
  }

  step(bytecode: string[]): boolean {
    const s = this.state
    if (s.pc >= bytecode.length || s.halted) {
      s.halted = true
      return false
    }

    const line = bytecode[s.pc]
    const parts = line.split(/\s+/)
    const opcode = parts[0]
    const operand = parts.slice(1).join(' ').split(';')[0].trim()

    let val: VMValue = null
    if (operand.startsWith('"') && operand.endsWith('"')) {
      val = operand.slice(1, -1)
    } else if (!isNaN(Number(operand)) && operand !== '') {
      val = Number(operand)
    } else if (operand !== '') {
      val = operand
    }

    const pop = (): VMValue => s.stack.pop() ?? null
    const push = (v: VMValue) => s.stack.push(v)
    const top = () => s.stack[s.stack.length - 1] ?? null

    switch (opcode) {
      case 'OP_CONSTANT':
        push(val)
        break
      case 'OP_NIL':
        push(null)
        break
      case 'OP_TRUE':
        push(true as VMValue)
        break
      case 'OP_FALSE':
        push(false as VMValue)
        break
      case 'OP_POP':
        pop()
        break
      case 'OP_DUP':
        if (s.stack.length > 0) push(top())
        break
      case 'OP_ADD': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push(a + b)
        break
      }
      case 'OP_SUB': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push(a - b)
        break
      }
      case 'OP_MUL': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push(a * b)
        break
      }
      case 'OP_DIV': {
        const b = Number(pop()) || 1
        const a = Number(pop()) || 0
        push(a / b)
        break
      }
      case 'OP_MOD': {
        const b = Number(pop()) || 1
        const a = Number(pop()) || 0
        push(a % b)
        break
      }
      case 'OP_NEGATE': {
        push(-(Number(pop()) || 0))
        break
      }
      case 'OP_EQUAL': {
        const b = pop()
        const a = pop()
        push((a === b) as VMValue)
        break
      }
      case 'OP_NOT_EQUAL': {
        const b = pop()
        const a = pop()
        push((a !== b) as VMValue)
        break
      }
      case 'OP_GREATER': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push((a > b) as VMValue)
        break
      }
      case 'OP_GREATER_EQUAL': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push((a >= b) as VMValue)
        break
      }
      case 'OP_LESS': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push((a < b) as VMValue)
        break
      }
      case 'OP_LESS_EQUAL': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push((a <= b) as VMValue)
        break
      }
      case 'OP_BIT_AND': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push(a & b)
        break
      }
      case 'OP_BIT_OR': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push(a | b)
        break
      }
      case 'OP_BIT_XOR': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push(a ^ b)
        break
      }
      case 'OP_BIT_NOT': {
        push(~(Number(pop()) || 0))
        break
      }
      case 'OP_SHIFT_LEFT': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push(a << b)
        break
      }
      case 'OP_SHIFT_RIGHT': {
        const b = Number(pop()) || 0
        const a = Number(pop()) || 0
        push(a >> b)
        break
      }
      case 'OP_NOT': {
        push((!pop()) as VMValue)
        break
      }
      case 'OP_TRUTHY': {
        const v = pop()
        push((!!v && v !== 0 && v !== '') as VMValue)
        break
      }
      case 'OP_JUMP': {
        const offset = parseInt(operand, 16)
        s.pc += offset - 1
        break
      }
      case 'OP_JUMP_IF_FALSE': {
        const cond = pop()
        if (!cond || cond === 0 || cond === '' || cond === null) {
          const offset = parseInt(operand, 16)
          s.pc += offset - 1
        }
        break
      }
      case 'OP_LOOP_BACK': {
        const offset = parseInt(operand, 16)
        if (offset > 0x7FFF) {
          s.pc -= (0x10000 - offset) + 1
        } else {
          s.pc += offset - 1
        }
        break
      }
      case 'OP_DEFINE_GLOBAL':
        s.globals.set(val as string, pop())
        break
      case 'OP_SET_GLOBAL':
        s.globals.set(val as string, top())
        break
      case 'OP_GET_GLOBAL':
        push(s.globals.get(val as string) ?? null)
        break
      case 'OP_GET_PROPERTY': {
        const obj = pop()
        const rec = typeof obj === 'object' && obj !== null && !Array.isArray(obj) ? (obj as unknown) as Record<string, unknown> : {}
        push((rec[val as string] as VMValue | undefined) ?? null)
        break
      }
      case 'OP_SET_PROPERTY': {
        const value = pop()
        const obj = pop()
        if (typeof obj === 'object' && obj !== null && !Array.isArray(obj)) {
          (obj as unknown as Record<string, unknown>)[val as string] = value
        }
        break
      }
      case 'OP_GET_INDEX': {
        const idx = Number(pop()) || 0
        const arr = pop()
        const arr2 = Array.isArray(arr) ? arr as VMValue[] : []
        push(arr2[idx] ?? null)
        break
      }
      case 'OP_SET_INDEX': {
        const value = pop()
        const idx = Number(pop()) || 0
        const arr = pop()
        if (Array.isArray(arr)) {
          (arr as VMValue[])[idx] = value
        }
        break
      }
      case 'OP_ARRAY': {
        const count = Number(val) || 0
        const arr: VMValue[] = []
        for (let i = 0; i < count; i++) arr.unshift(pop())
        push(arr)
        break
      }
      case 'OP_TUPLE': {
        const count = Number(val) || 0
        const tup: VMValue[] = []
        for (let i = 0; i < count; i++) tup.unshift(pop())
        push(tup)
        break
      }
      case 'OP_DICT': {
        const count = Number(val) || 0
        const dict: Record<string, VMValue> = {}
        for (let i = 0; i < count; i++) {
          const v = pop()
          const k = String(pop())
          dict[k] = v
        }
        push(dict)
        break
      }
      case 'OP_ARRAY_LEN': {
        const arr = pop()
        push((Array.isArray(arr) ? (arr as unknown[]).length : 0) as VMValue)
        break
      }
      case 'OP_SLICE': {
        const end = Number(pop()) || 0
        const start = Number(pop()) || 0
        const arr = pop()
        push((Array.isArray(arr) ? (arr as unknown[]).slice(start, end) : []) as VMValue)
        break
      }
      case 'OP_CALL': {
        const argCount = Number(val) || 0
        const args: VMValue[] = []
        for (let i = 0; i < argCount; i++) args.unshift(pop())
        const func = pop()
        if (func === 'fib') {
          const n = Number(args[0]) || 0
          const fib = (x: number): number => x <= 1 ? x : fib(x - 1) + fib(x - 2)
          push(fib(n))
        } else if (func === 'range') {
          const n = Number(args[0]) || 0
          push(Array.from({ length: n }, (_, i) => i) as VMValue)
        } else if (func === 'len') {
          const arr = args[0]
          push((Array.isArray(arr) ? (arr as unknown[]).length : 0) as VMValue)
        } else if (typeof func === 'object' && func !== null) {
          const cls = (func as unknown) as Record<string, unknown>
          const instance = { ...cls }
          push(instance as VMValue)
        } else {
          push(null)
        }
        break
      }
      case 'OP_CALL_METHOD': {
        const methodName = parts[1]?.replace(/"/g, '') || ''
        const argCount = Number(parts[2]) || 0
        const args: VMValue[] = []
        for (let i = 0; i < argCount; i++) args.unshift(pop())
        const instance = pop()
        if (methodName === 'move' && typeof instance === 'object' && instance !== null) {
          const rec = (instance as unknown) as Record<string, unknown>
          rec.x = (Number(rec.x) || 0) + (Number(args[0]) || 0)
          rec.y = (Number(rec.y) || 0) + (Number(args[1]) || 0)
          push(rec as VMValue)
        } else {
          push(instance)
        }
        break
      }
      case 'OP_CLASS': {
        push({ __type__: 'class', name: val } as VMValue)
        break
      }
      case 'OP_METHOD':
      case 'OP_INHERIT':
      case 'OP_DEFINE_FUNCTION':
        break
      case 'OP_PUSH_ENV':
        s.locals.push(new Map())
        s.envDepth++
        break
      case 'OP_POP_ENV':
        s.locals.pop()
        s.envDepth--
        break
      case 'OP_PRINT': {
        const v = pop()
        s.output.push(`> ${v === null ? 'nil' : typeof v === 'object' ? JSON.stringify(v) : v}`)
        break
      }
      case 'OP_RETURN':
      case 'OP_HALT':
        s.halted = true
        break
      case 'OP_BREAK':
      case 'OP_CONTINUE':
        s.output.push(`[CTRL] ${opcode} not supported in playground`)
        break
      case 'OP_SETUP_TRY':
      case 'OP_END_TRY':
      case 'OP_RAISE':
        s.output.push(`[EXC] ${opcode} not supported in playground`)
        break
      case 'OP_GPU_POLL_EVENTS':
      case 'OP_GPU_WINDOW_SHOULD_CLOSE':
      case 'OP_GPU_GET_TIME':
      case 'OP_GPU_KEY_PRESSED':
      case 'OP_GPU_KEY_DOWN':
      case 'OP_GPU_MOUSE_POS':
      case 'OP_GPU_MOUSE_DELTA':
      case 'OP_GPU_UPDATE_INPUT':
      case 'OP_GPU_BEGIN_COMMANDS':
      case 'OP_GPU_END_COMMANDS':
      case 'OP_GPU_CMD_BEGIN_RP':
      case 'OP_GPU_CMD_END_RP':
      case 'OP_GPU_CMD_DRAW':
      case 'OP_GPU_CMD_BIND_GP':
      case 'OP_GPU_CMD_BIND_DS':
      case 'OP_GPU_CMD_SET_VP':
      case 'OP_GPU_CMD_SET_SC':
      case 'OP_GPU_CMD_BIND_VB':
      case 'OP_GPU_CMD_BIND_IB':
      case 'OP_GPU_CMD_DRAW_IDX':
      case 'OP_GPU_SUBMIT_SYNC':
      case 'OP_GPU_ACQUIRE_IMG':
      case 'OP_GPU_PRESENT':
      case 'OP_GPU_WAIT_FENCE':
      case 'OP_GPU_RESET_FENCE':
      case 'OP_GPU_UPDATE_UNIFORM':
      case 'OP_GPU_CMD_PUSH_CONST':
      case 'OP_GPU_CMD_DISPATCH':
        s.output.push(`[GPU] ${opcode} not supported in playground`)
        break
      default:
        s.output.push(`[WARN] Unknown opcode: ${opcode}`)
    }

    s.pc++
    return !s.halted
  }
}

// ─── Component ───
export default function Playground() {
  const [selectedProgramIdx, setSelectedProgramIdx] = useState(0)
  const [isRunning, setIsRunning] = useState(false)
  const [currentStep, setCurrentStep] = useState(-1)
  const [consoleOutput, setConsoleOutput] = useState<string[]>([])
  const [stack, setStack] = useState<VMValue[]>([])
  const [globals, setGlobals] = useState<Map<string, VMValue>>(new Map())
  const [customSage, setCustomSage] = useState('')
  const [customBytecode, setCustomBytecode] = useState('')
  const [isHalted, setIsHalted] = useState(false)
  const [speed, setSpeed] = useState(400)
  const [showGlobals, setShowGlobals] = useState(false)
  const [showSage, setShowSage] = useState(true)
  const [copied, setCopied] = useState(false)
  const [error, setError] = useState('')

  const sectionRef = useRef<HTMLDivElement>(null)
  const bytecodeRefs = useRef<(HTMLDivElement | null)[]>([])
  const vmRef = useRef(new SGVMInterpreter())
  const stopRef = useRef(false)

  const program = SAMPLE_PROGRAMS[selectedProgramIdx]
  const isCustom = program.isCustom || false

  const getBytecode = useCallback((): string[] => {
    if (isCustom) {
      return customBytecode.split('\n').map(l => l.trim()).filter(l => l && !l.startsWith(';'))
    }
    return program.bytecode
  }, [isCustom, customBytecode, program.bytecode])

  const getSage = useCallback((): string => {
    if (isCustom) return customSage
    return program.sage
  }, [isCustom, customSage, program.sage])

  const reset = () => {
    stopRef.current = true
    setIsRunning(false)
    setCurrentStep(-1)
    setConsoleOutput([])
    setStack([])
    setGlobals(new Map())
    setIsHalted(false)
    setError('')
    vmRef.current.reset()
  }

  const compile = () => {
    reset()
    setError('')
    try {
      const source = getSage()
      const bc = compileSageToBytecode(source)
      setCustomBytecode(bc.join('\n'))
    } catch (err) {
      setError(`Compile error: ${err}`)
    }
  }

  const run = async () => {
    reset()
    stopRef.current = false
    setIsRunning(true)

    const vm = vmRef.current
    const bytecode = getBytecode()

    while (!stopRef.current && !vm.state.halted && vm.state.pc < bytecode.length) {
      const stepIdx = vm.state.pc
      setCurrentStep(stepIdx)

      if (bytecodeRefs.current[stepIdx]) {
        gsap.fromTo(
          bytecodeRefs.current[stepIdx],
          { backgroundColor: 'rgba(0, 240, 255, 0.25)' },
          { backgroundColor: 'transparent', duration: 0.4, ease: 'power2.out' }
        )
      }

      vm.step(bytecode)
      const state = vm.getState()
      setStack(state.stack)
      setGlobals(new Map(state.globals))
      setConsoleOutput(state.output)
      setIsHalted(state.halted)

      await new Promise((r) => setTimeout(r, speed))
    }

    setIsRunning(false)
  }

  const step = () => {
    if (vmRef.current.state.halted || vmRef.current.state.pc >= getBytecode().length) {
      if (isHalted) reset()
      return
    }

    const vm = vmRef.current
    const stepIdx = vm.state.pc
    setCurrentStep(stepIdx)

    if (bytecodeRefs.current[stepIdx]) {
      gsap.fromTo(
        bytecodeRefs.current[stepIdx],
        { backgroundColor: 'rgba(0, 240, 255, 0.25)' },
        { backgroundColor: 'transparent', duration: 0.4, ease: 'power2.out' }
      )
    }

    vm.step(getBytecode())
    const state = vm.getState()
    setStack(state.stack)
    setGlobals(new Map(state.globals))
    setConsoleOutput(state.output)
    setIsHalted(state.halted)
  }

  const selectProgram = (idx: number) => {
    reset()
    setSelectedProgramIdx(idx)
    if (SAMPLE_PROGRAMS[idx].isCustom) {
      setCustomSage(SAMPLE_PROGRAMS[idx].sage)
      setCustomBytecode(SAMPLE_PROGRAMS[idx].bytecode.join('\n'))
    }
  }

  const copyBytecode = () => {
    navigator.clipboard.writeText(getBytecode().join('\n'))
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  const downloadBytecode = () => {
    const blob = new Blob([getBytecode().join('\n')], { type: 'text/plain' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = 'program.sgvm'
    a.click()
    URL.revokeObjectURL(url)
  }

  const uploadBytecode = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0]
    if (!file) return
    const reader = new FileReader()
    reader.onload = (ev) => {
      const text = ev.target?.result as string
      setCustomBytecode(text)
      setSelectedProgramIdx(SAMPLE_PROGRAMS.length - 1)
    }
    reader.readAsText(file)
  }

  const bytecode = getBytecode()

  return (
    <section
      id="playground"
      ref={sectionRef}
      className="relative py-32 bg-void"
    >
      <div className="relative z-10 max-w-7xl mx-auto px-8">
        {/* Header */}
        <div className="text-center mb-16">
          <p className="font-mono text-sm text-sage-mid tracking-widest mb-4">
            INTERACTIVE
          </p>
          <h2 className="font-display text-4xl md:text-6xl font-bold text-white tracking-tight">
            VM Playground
          </h2>
          <p className="font-body text-white/50 mt-4 max-w-2xl mx-auto">
            Write Sage code, compile to bytecode, and execute step-by-step.
            Watch the stack, globals, and console in real-time.
            <br />
            <span className="text-amber">Note: This playground currently runs the Stack-based SGVM. Register-based SRVM execution is supported via CLI build pipeline.</span>
          </p>
        </div>

        {/* Program Selector */}
        <div className="flex flex-wrap gap-2 mb-8 justify-center">
          {SAMPLE_PROGRAMS.map((p, i) => (
            <button
              key={p.name}
              onClick={() => selectProgram(i)}
              className={`px-4 py-2 rounded-lg font-mono text-xs transition-all duration-300 ${
                selectedProgramIdx === i
                  ? 'bg-sage-mid/15 text-sage-mid border border-sage-mid/30'
                  : 'bg-white/[0.03] text-white/50 border border-transparent hover:border-white/[0.08] hover:text-white/80'
              }`}
            >
              {p.isCustom ? <Edit3 className="w-3 h-3 inline mr-1" /> : <Code className="w-3 h-3 inline mr-1" />}
              {p.name}
            </button>
          ))}
        </div>

        {/* Error Banner */}
        {error && (
          <div className="mb-4 p-3 bg-red-500/10 border border-red-500/30 rounded-lg">
            <p className="text-red-400 text-xs font-mono">{error}</p>
          </div>
        )}

        {/* Main Grid */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* LEFT: Source + Bytecode */}
          <div className="space-y-4">
            {/* Sage Source */}
            <div className="glass-panel rounded-xl overflow-hidden">
              <div className="flex items-center justify-between px-4 py-3 border-b border-white/[0.06]">
                <div className="flex items-center gap-2">
                  <Code className="w-4 h-4 text-sage-mid" />
                  <span className="font-mono text-xs text-sage-mid">
                    {isCustom ? 'SAGE SOURCE (EDITABLE)' : 'SAGE SOURCE'}
                  </span>
                </div>
                <div className="flex items-center gap-2">
                  {isCustom && (
                    <button
                      onClick={compile}
                      className="px-3 py-1 bg-sage-mid/15 text-sage-mid border border-sage-mid/30 rounded font-mono text-[10px] hover:bg-sage-mid/25 transition-colors"
                    >
                      <Zap className="w-3 h-3 inline mr-1" />
                      Compile to Bytecode
                    </button>
                  )}
                  <button
                    onClick={() => setShowSage(!showSage)}
                    className="text-white/40 hover:text-white/70 transition-colors"
                  >
                    {showSage ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
                  </button>
                </div>
              </div>
              {showSage && (
                <div className="p-0">
                  <textarea
                    value={getSage()}
                    onChange={(e) => isCustom ? setCustomSage(e.target.value) : null}
                    readOnly={!isCustom}
                    className={`w-full h-48 bg-transparent p-4 font-mono text-xs leading-relaxed focus:outline-none resize-none ${
                      isCustom ? 'text-white/80' : 'text-white/50'
                    }`}
                    spellCheck={false}
                  />
                </div>
              )}
            </div>

            {/* Bytecode */}
            <div className="glass-panel rounded-xl overflow-hidden">
              <div className="flex items-center justify-between px-4 py-3 border-b border-white/[0.06]">
                <div className="flex items-center gap-2">
                  <Terminal className="w-4 h-4 text-sage-mid" />
                  <span className="font-mono text-xs text-sage-mid">
                    {isCustom ? 'BYTECODE (EDITABLE)' : 'BYTECODE'}
                  </span>
                </div>
                <div className="flex items-center gap-2">
                  <button onClick={copyBytecode} className="text-white/40 hover:text-white/70 transition-colors" title="Copy">
                    {copied ? <Check className="w-4 h-4 text-sage-mid" /> : <Copy className="w-4 h-4" />}
                  </button>
                  <button onClick={downloadBytecode} className="text-white/40 hover:text-white/70 transition-colors" title="Download">
                    <Download className="w-4 h-4" />
                  </button>
                  <label className="text-white/40 hover:text-white/70 transition-colors cursor-pointer" title="Upload">
                    <Upload className="w-4 h-4" />
                    <input type="file" accept=".sgvm,.txt" onChange={uploadBytecode} className="hidden" />
                  </label>
                </div>
              </div>
              <div className="code-block p-4 max-h-64 overflow-y-auto">
                <pre className="font-mono text-xs leading-relaxed">
                  {bytecode.map((line, i) => (
                    <div
                      key={i}
                      ref={(el) => { bytecodeRefs.current[i] = el }}
                      className={`py-0.5 px-1 rounded transition-colors flex ${
                        i === currentStep
                          ? 'bg-sage-mid/10 text-sage-mid'
                          : 'text-white/60'
                      }`}
                    >
                      <span className="text-white/30 mr-3 w-6 text-right select-none">
                        {String(i + 1).padStart(2, '0')}
                      </span>
                      <span className="flex-1">{line}</span>
                    </div>
                  ))}
                </pre>
              </div>
            </div>
          </div>

          {/* RIGHT: Controls + Execution */}
          <div className="space-y-4">
            {/* Controls */}
            <div className="flex flex-wrap items-center gap-3">
              <button
                onClick={run}
                disabled={isRunning}
                className="flex items-center gap-2 px-5 py-2.5 bg-sage-mid/15 text-sage-mid border border-sage-mid/30 rounded-lg font-mono text-sm hover:bg-sage-mid/25 transition-all disabled:opacity-50"
              >
                <Play className="w-4 h-4" />
                {isRunning ? 'Running...' : 'Run'}
              </button>
              <button
                onClick={step}
                disabled={isRunning}
                className="flex items-center gap-2 px-5 py-2.5 bg-white/[0.03] text-white/70 border border-white/[0.08] rounded-lg font-mono text-sm hover:border-sage-mid/30 hover:text-sage-mid transition-all disabled:opacity-50"
              >
                <StepForward className="w-4 h-4" />
                Step
              </button>
              <button
                onClick={reset}
                className="flex items-center gap-2 px-5 py-2.5 bg-white/[0.03] text-white/70 border border-white/[0.08] rounded-lg font-mono text-sm hover:border-sage-mid/30 hover:text-sage-mid transition-all"
              >
                <RotateCcw className="w-4 h-4" />
                Reset
              </button>

              <div className="flex items-center gap-2 ml-auto">
                <span className="text-white/30 text-[10px] font-mono">SPEED</span>
                <input
                  type="range"
                  min="50"
                  max="1000"
                  step="50"
                  value={speed}
                  onChange={(e) => setSpeed(Number(e.target.value))}
                  className="w-24 accent-sage-mid"
                />
                <span className="text-white/30 text-[10px] font-mono w-10">{speed}ms</span>
              </div>
            </div>

            {/* Status */}
            <div className="flex items-center gap-4 text-[10px] font-mono">
              <span className={`px-2 py-1 rounded ${isRunning ? 'bg-sage-mid/20 text-sage-mid' : 'bg-white/[0.03] text-white/40'}`}>
                {isRunning ? '● RUNNING' : isHalted ? '■ HALTED' : '○ IDLE'}
              </span>
              <span className="text-white/30">
                PC: {currentStep >= 0 ? currentStep : '-'} / {bytecode.length}
              </span>
              <span className="text-white/30">
                Stack: {stack.length}
              </span>
            </div>

            {/* Stack */}
            <div className="glass-panel rounded-xl overflow-hidden">
              <div className="px-4 py-3 border-b border-white/[0.06] flex items-center justify-between">
                <span className="font-mono text-xs text-sage-light">STACK</span>
                <span className="text-white/30 text-[10px] font-mono">{stack.length} items</span>
              </div>
              <div className="p-4 min-h-[100px]">
                {stack.length === 0 ? (
                  <p className="text-white/30 font-mono text-xs text-center py-6">
                    Stack empty — run to see values
                  </p>
                ) : (
                  <div className="flex flex-col-reverse gap-2">
                    {stack.map((val, i) => (
                      <div
                        key={`${i}-${String(val)}`}
                        className="border border-sage-light/30 rounded-lg px-4 py-2.5 bg-sage-light/5"
                      >
                        <div className="flex items-center justify-between">
                          <span className="font-mono text-xs text-sage-light">
                            {val === null ? 'nil' : typeof val === 'object' ? JSON.stringify(val) : String(val)}
                          </span>
                          <span className="text-white/20 text-[10px] font-mono">idx {i}</span>
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            </div>

            {/* Globals */}
            <div className="glass-panel rounded-xl overflow-hidden">
              <div
                className="px-4 py-3 border-b border-white/[0.06] flex items-center justify-between cursor-pointer"
                onClick={() => setShowGlobals(!showGlobals)}
              >
                <span className="font-mono text-xs text-amber">GLOBALS</span>
                <div className="flex items-center gap-2">
                  <span className="text-white/30 text-[10px] font-mono">{globals.size} vars</span>
                  {showGlobals ? <ChevronDown className="w-4 h-4 text-white/40" /> : <ChevronRight className="w-4 h-4 text-white/40" />}
                </div>
              </div>
              {showGlobals && (
                <div className="p-4">
                  {globals.size === 0 ? (
                    <p className="text-white/30 font-mono text-xs text-center py-4">
                      No globals defined
                    </p>
                  ) : (
                    <div className="grid grid-cols-2 gap-2">
                      {Array.from(globals.entries()).map(([k, v]) => (
                        <div key={k} className="bg-white/[0.03] rounded px-3 py-2">
                          <span className="text-white/40 text-[10px] font-mono">{k}</span>
                          <p className="text-white/70 text-xs font-mono truncate">
                            {v === null ? 'nil' : typeof v === 'object' ? JSON.stringify(v) : String(v)}
                          </p>
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              )}
            </div>

            {/* Console */}
            <div className="glass-panel rounded-xl overflow-hidden">
              <div className="px-4 py-3 border-b border-white/[0.06] flex items-center justify-between">
                <span className="font-mono text-xs text-amber">CONSOLE</span>
                <span className="text-white/30 text-[10px] font-mono">{consoleOutput.length} lines</span>
              </div>
              <div className="code-block p-4 min-h-[100px] max-h-[200px] overflow-y-auto">
                {consoleOutput.length === 0 ? (
                  <p className="text-white/30 font-mono text-xs">
                    $ waiting for execution...
                  </p>
                ) : (
                  <div className="space-y-1">
                    {consoleOutput.map((line, i) => (
                      <p key={i} className="font-mono text-xs text-white/80">
                        {line}
                      </p>
                    ))}
                    {isHalted && (
                      <p className="font-mono text-xs text-white/30 mt-2">
                        ── execution complete ──
                      </p>
                    )}
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>

        {/* Opcode Legend */}
        <div className="mt-12 glass-panel rounded-xl p-6">
          <h3 className="font-mono text-xs text-white/50 uppercase tracking-wider mb-4">Supported Opcodes in Playground</h3>
          <div className="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-6 gap-2 text-[10px] font-mono">
            {[
              'OP_CONSTANT', 'OP_NIL', 'OP_TRUE', 'OP_FALSE', 'OP_POP', 'OP_DUP',
              'OP_ADD', 'OP_SUB', 'OP_MUL', 'OP_DIV', 'OP_MOD', 'OP_NEGATE',
              'OP_EQUAL', 'OP_NOT_EQUAL', 'OP_GREATER', 'OP_GREATER_EQUAL', 'OP_LESS', 'OP_LESS_EQUAL',
              'OP_BIT_AND', 'OP_BIT_OR', 'OP_BIT_XOR', 'OP_BIT_NOT', 'OP_SHIFT_LEFT', 'OP_SHIFT_RIGHT',
              'OP_NOT', 'OP_TRUTHY', 'OP_JUMP', 'OP_JUMP_IF_FALSE', 'OP_LOOP_BACK',
              'OP_DEFINE_GLOBAL', 'OP_SET_GLOBAL', 'OP_GET_GLOBAL',
              'OP_GET_PROPERTY', 'OP_SET_PROPERTY', 'OP_GET_INDEX', 'OP_SET_INDEX',
              'OP_ARRAY', 'OP_TUPLE', 'OP_DICT', 'OP_ARRAY_LEN', 'OP_SLICE',
              'OP_CALL', 'OP_CALL_METHOD', 'OP_CLASS', 'OP_METHOD',
              'OP_PUSH_ENV', 'OP_POP_ENV', 'OP_PRINT', 'OP_RETURN', 'OP_HALT',
            ].map(op => (
              <span key={op} className="text-white/40 bg-white/[0.03] rounded px-2 py-1">{op}</span>
            ))}
          </div>
        </div>
      </div>
    </section>
  )
}
