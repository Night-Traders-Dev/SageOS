import { useState, useRef, useEffect } from 'react'
import gsap from 'gsap'
import { ScrollTrigger } from 'gsap/ScrollTrigger'
import { Search, Terminal } from 'lucide-react'

gsap.registerPlugin(ScrollTrigger)

type Category = 'all' | 'stack' | 'arithmetic' | 'comparison' | 'bitwise' | 'control' | 'memory' | 'oop' | 'exception' | 'gpu' | 'math' | 'srvm'

interface Opcode {
  name: string
  value: string
  description: string
  category: Category
  example: string
}

const categories: { id: Category; label: string }[] = [
  { id: 'all', label: 'All' },
  { id: 'stack', label: 'Stack (SGVM)' },
  { id: 'arithmetic', label: 'Arithmetic' },
  { id: 'comparison', label: 'Comparison' },
  { id: 'bitwise', label: 'Bitwise' },
  { id: 'control', label: 'Control Flow' },
  { id: 'memory', label: 'Memory' },
  { id: 'oop', label: 'OOP' },
  { id: 'exception', label: 'Exception' },
  { id: 'gpu', label: 'GPU' },
  { id: 'math', label: 'Math' },
  { id: 'srvm', label: 'Register (SRVM)' },
]

const opcodes: Opcode[] = [
  // Stack Operations (0x00-0x04, 0x2F, 0x2A)
  { name: 'OP_CONSTANT', value: '0x00', description: 'Push a constant from the constant pool onto the stack', category: 'stack', example: 'OP_CONSTANT 0x0005  ; Push constants[5] onto stack' },
  { name: 'OP_NIL', value: '0x01', description: 'Push nil onto the stack', category: 'stack', example: 'OP_NIL  ; Stack: [..., nil]' },
  { name: 'OP_TRUE', value: '0x02', description: 'Push true onto the stack', category: 'stack', example: 'OP_TRUE  ; Stack: [..., true]' },
  { name: 'OP_FALSE', value: '0x03', description: 'Push false onto the stack', category: 'stack', example: 'OP_FALSE  ; Stack: [..., false]' },
  { name: 'OP_POP', value: '0x04', description: 'Pop the top value from the stack and discard it', category: 'stack', example: 'OP_POP  ; Stack: [1, 2, 3] -> [1, 2]' },
  { name: 'OP_DUP', value: '0x2F', description: 'Duplicate the top value on the stack', category: 'stack', example: 'OP_DUP  ; Stack: [..., 5] -> [..., 5, 5]' },
  { name: 'OP_PRINT', value: '0x2A', description: 'Pop and print the top value to stdout', category: 'stack', example: 'OP_PRINT  ; Pop value, print it, push nil' },

  // Memory / Variable Operations (0x05-0x0C, 0x30, 0x2D, 0x2E)
  { name: 'OP_GET_GLOBAL', value: '0x05', description: 'Read a global variable and push its value', category: 'memory', example: 'OP_GET_GLOBAL 0x0003  ; Push globals["count"]' },
  { name: 'OP_DEFINE_GLOBAL', value: '0x06', description: 'Pop a value and define a new global variable', category: 'memory', example: 'OP_DEFINE_GLOBAL 0x0001  ; globals["x"] = pop()' },
  { name: 'OP_SET_GLOBAL', value: '0x07', description: 'Pop a value and assign it to an existing global', category: 'memory', example: 'OP_SET_GLOBAL 0x0002  ; globals["y"] = pop()' },
  { name: 'OP_GET_PROPERTY', value: '0x09', description: 'Get an object property by name from the constant pool', category: 'memory', example: 'OP_GET_PROPERTY 0x0004  ; obj.name -> push value' },
  { name: 'OP_SET_PROPERTY', value: '0x0A', description: 'Set an object property: pop value, pop object', category: 'memory', example: 'OP_SET_PROPERTY 0x0005  ; obj.prop = value' },
  { name: 'OP_GET_INDEX', value: '0x0B', description: 'Get array/dict element: pop index, pop container', category: 'memory', example: 'OP_GET_INDEX  ; arr[3] -> push value at index' },
  { name: 'OP_SET_INDEX', value: '0x0C', description: 'Set array/dict element: pop value, pop index, pop container', category: 'memory', example: 'OP_SET_INDEX  ; arr[0] = 42' },
  { name: 'OP_ARRAY_LEN', value: '0x30', description: 'Push the length of the array on top of stack', category: 'memory', example: 'OP_ARRAY_LEN  ; [1,2,3] -> push 3' },
  { name: 'OP_PUSH_ENV', value: '0x2D', description: 'Push a new local variable scope (environment)', category: 'memory', example: 'OP_PUSH_ENV  ; Enter function scope' },
  { name: 'OP_POP_ENV', value: '0x2E', description: 'Pop the current local variable scope', category: 'memory', example: 'OP_POP_ENV  ; Exit function scope' },

  // Function / Control Flow (0x08, 0x0D, 0x25, 0x26, 0x2C, 0x23, 0x24, 0x31-0x34)
  { name: 'OP_DEFINE_FUNCTION', value: '0x08', description: 'Define a named function with chunk index and arity', category: 'control', example: 'OP_DEFINE_FUNCTION 0x0001 0x02  ; func foo(a,b) at chunk 1' },
  { name: 'OP_LOAD_FUNCTION', value: '0x0D', description: 'Load a function value onto the stack by chunk index', category: 'control', example: 'OP_LOAD_FUNCTION 0x0002  ; Push function ref for chunk 2' },
  { name: 'OP_CALL', value: '0x25', description: 'Call a function with N arguments from the stack', category: 'control', example: 'OP_CALL 0x02  ; Call with 2 args: func(arg1, arg2)' },
  { name: 'OP_CALL_METHOD', value: '0x26', description: 'Call an object method: pop args, pop method name, pop instance', category: 'control', example: 'OP_CALL_METHOD 0x01  ; obj.method(arg)' },
  { name: 'OP_RETURN', value: '0x2C', description: 'Return from current function, optionally with a value', category: 'control', example: 'OP_RETURN  ; Pop return value, restore caller IP' },
  { name: 'OP_JUMP', value: '0x23', description: 'Unconditional relative jump by signed 16-bit offset', category: 'control', example: 'OP_JUMP 0x0010  ; PC += 16 (forward 16 bytes)' },
  { name: 'OP_JUMP_IF_FALSE', value: '0x24', description: 'Conditional jump: pop value, jump if falsy', category: 'control', example: 'OP_JUMP_IF_FALSE 0xFFEC  ; if !cond: PC -= 20' },
  { name: 'OP_BREAK', value: '0x31', description: 'Break out of the current loop', category: 'control', example: 'OP_BREAK  ; Jump to loop end (emitted by compiler)' },
  { name: 'OP_CONTINUE', value: '0x32', description: 'Continue to the next loop iteration', category: 'control', example: 'OP_CONTINUE  ; Jump to loop start (emitted by compiler)' },
  { name: 'OP_LOOP_BACK', value: '0x33', description: 'Jump backward to the start of a loop', category: 'control', example: 'OP_LOOP_BACK 0xFFD0  ; PC -= 48 (loop back)' },
  { name: 'OP_IMPORT', value: '0x34', description: 'Import a module by name from the constant pool', category: 'control', example: 'OP_IMPORT 0x0007  ; import "math" -> push module dict' },
  { name: 'OP_EXEC_AST_STMT', value: '0x2B', description: 'Execute a statement via AST interpreter fallback', category: 'control', example: 'OP_EXEC_AST_STMT 0x000A  ; Fallback for unsupported constructs' },

  // Arithmetic (0x0F-0x14)
  { name: 'OP_ADD', value: '0x0F', description: 'Pop two values, add them, push result', category: 'arithmetic', example: 'OP_ADD  ; Stack: [3, 4] -> [7]' },
  { name: 'OP_SUB', value: '0x10', description: 'Pop two values, subtract top from second, push result', category: 'arithmetic', example: 'OP_SUB  ; Stack: [10, 3] -> [7]' },
  { name: 'OP_MUL', value: '0x11', description: 'Pop two values, multiply them, push result', category: 'arithmetic', example: 'OP_MUL  ; Stack: [6, 7] -> [42]' },
  { name: 'OP_DIV', value: '0x12', description: 'Pop two values, divide second by top, push result', category: 'arithmetic', example: 'OP_DIV  ; Stack: [10, 2] -> [5.0]' },
  { name: 'OP_MOD', value: '0x13', description: 'Pop two values, modulo second by top, push result', category: 'arithmetic', example: 'OP_MOD  ; Stack: [10, 3] -> [1]' },
  { name: 'OP_NEGATE', value: '0x14', description: 'Pop one value, negate it, push result', category: 'arithmetic', example: 'OP_NEGATE  ; Stack: [5] -> [-5]' },

  // Comparison (0x15-0x1A, 0x22)
  { name: 'OP_EQUAL', value: '0x15', description: 'Pop two values, compare equality, push bool', category: 'comparison', example: 'OP_EQUAL  ; Stack: [5, 5] -> [true]' },
  { name: 'OP_NOT_EQUAL', value: '0x16', description: 'Pop two values, compare inequality, push bool', category: 'comparison', example: 'OP_NOT_EQUAL  ; Stack: [5, 3] -> [true]' },
  { name: 'OP_GREATER', value: '0x17', description: 'Pop two values, check second > top, push bool', category: 'comparison', example: 'OP_GREATER  ; Stack: [5, 3] -> [true]' },
  { name: 'OP_GREATER_EQUAL', value: '0x18', description: 'Pop two values, check second >= top, push bool', category: 'comparison', example: 'OP_GREATER_EQUAL  ; Stack: [5, 5] -> [true]' },
  { name: 'OP_LESS', value: '0x19', description: 'Pop two values, check second < top, push bool', category: 'comparison', example: 'OP_LESS  ; Stack: [3, 5] -> [true]' },
  { name: 'OP_LESS_EQUAL', value: '0x1A', description: 'Pop two values, check second <= top, push bool', category: 'comparison', example: 'OP_LESS_EQUAL  ; Stack: [5, 5] -> [true]' },
  { name: 'OP_TRUTHY', value: '0x22', description: 'Pop one value, check if truthy, push bool', category: 'comparison', example: 'OP_TRUTHY  ; Stack: [0] -> [false] (0 is falsy)' },

  // Bitwise (0x1B-0x21)
  { name: 'OP_BIT_AND', value: '0x1B', description: 'Pop two values, bitwise AND, push result', category: 'bitwise', example: 'OP_BIT_AND  ; Stack: [0b1100, 0b1010] -> [0b1000]' },
  { name: 'OP_BIT_OR', value: '0x1C', description: 'Pop two values, bitwise OR, push result', category: 'bitwise', example: 'OP_BIT_OR  ; Stack: [0b1100, 0b1010] -> [0b1110]' },
  { name: 'OP_BIT_XOR', value: '0x1D', description: 'Pop two values, bitwise XOR, push result', category: 'bitwise', example: 'OP_BIT_XOR  ; Stack: [0b1100, 0b1010] -> [0b0110]' },
  { name: 'OP_BIT_NOT', value: '0x1E', description: 'Pop one value, bitwise NOT (complement), push result', category: 'bitwise', example: 'OP_BIT_NOT  ; Stack: [0b00001111] -> [0b11110000]' },
  { name: 'OP_SHIFT_LEFT', value: '0x1F', description: 'Pop two values, shift second left by top bits', category: 'bitwise', example: 'OP_SHIFT_LEFT  ; Stack: [1, 3] -> [8] (1 << 3)' },
  { name: 'OP_SHIFT_RIGHT', value: '0x20', description: 'Pop two values, shift second right by top bits', category: 'bitwise', example: 'OP_SHIFT_RIGHT  ; Stack: [8, 1] -> [4] (8 >> 1)' },
  { name: 'OP_NOT', value: '0x21', description: 'Pop one value, logical NOT, push bool', category: 'bitwise', example: 'OP_NOT  ; Stack: [true] -> [false]' },

  // Data Structure Creation (0x27-0x29, 0x0E)
  { name: 'OP_ARRAY', value: '0x27', description: 'Create an array from N values on the stack', category: 'memory', example: 'OP_ARRAY 0x03  ; Stack: [1,2,3] -> [[1,2,3]]' },
  { name: 'OP_TUPLE', value: '0x28', description: 'Create a tuple from N values on the stack', category: 'memory', example: 'OP_TUPLE 0x02  ; Stack: [1,2] -> [(1,2)]' },
  { name: 'OP_DICT', value: '0x29', description: 'Create a dict from N key-value pairs on the stack', category: 'memory', example: 'OP_DICT 0x01  ; Stack: ["k",1] -> [{"k":1}]' },
  { name: 'OP_SLICE', value: '0x0E', description: 'Slice array/string: pop end, pop start, pop container', category: 'memory', example: 'OP_SLICE  ; arr[1:3] -> push sub-array' },

  // OOP (0x35-0x37)
  { name: 'OP_CLASS', value: '0x35', description: 'Define a new class with name from constant pool', category: 'oop', example: 'OP_CLASS 0x0008  ; class Point: ...' },
  { name: 'OP_METHOD', value: '0x36', description: 'Add a method to the current class definition', category: 'oop', example: 'OP_METHOD 0x0009  ; def move(self, x): ...' },
  { name: 'OP_INHERIT', value: '0x37', description: 'Set up class inheritance from superclass', category: 'oop', example: 'OP_INHERIT 0x000A  ; class B(A): ...' },

  // Exception Handling (0x38-0x3A)
  { name: 'OP_SETUP_TRY', value: '0x38', description: 'Push exception handler with catch offset and finally offset', category: 'exception', example: 'OP_SETUP_TRY 0x0020 0x0040  ; try { ... } catch { ... }' },
  { name: 'OP_END_TRY', value: '0x39', description: 'Pop the current exception handler frame', category: 'exception', example: 'OP_END_TRY  ; End of try/catch block' },
  { name: 'OP_RAISE', value: '0x3A', description: 'Pop exception value and raise it', category: 'exception', example: 'OP_RAISE  ; raise Exception("error")' },

  // GPU Opcodes (0x3B-0x56)
  { name: 'OP_GPU_POLL_EVENTS', value: '0x3B', description: 'Poll windowing system events (SDL/glfw)', category: 'gpu', example: 'OP_GPU_POLL_EVENTS  ; Process window events' },
  { name: 'OP_GPU_WINDOW_SHOULD_CLOSE', value: '0x3C', description: 'Push bool: should window close?', category: 'gpu', example: 'OP_GPU_WINDOW_SHOULD_CLOSE  ; while !window_should_close()' },
  { name: 'OP_GPU_GET_TIME', value: '0x3D', description: 'Push high-precision GPU/frame timer value', category: 'gpu', example: 'OP_GPU_GET_TIME  ; let t = gpu.get_time()' },
  { name: 'OP_GPU_KEY_PRESSED', value: '0x3E', description: 'Push bool: was key pressed this frame?', category: 'gpu', example: 'OP_GPU_KEY_PRESSED 0x0041  ; if key_pressed("A")' },
  { name: 'OP_GPU_KEY_DOWN', value: '0x3F', description: 'Push bool: is key currently held down?', category: 'gpu', example: 'OP_GPU_KEY_DOWN 0x001B  ; if key_down(ESC)' },
  { name: 'OP_GPU_MOUSE_POS', value: '0x40', description: 'Push mouse position as [x, y] tuple', category: 'gpu', example: 'OP_GPU_MOUSE_POS  ; let [x, y] = gpu.mouse_pos()' },
  { name: 'OP_GPU_MOUSE_DELTA', value: '0x41', description: 'Push mouse movement delta as [dx, dy]', category: 'gpu', example: 'OP_GPU_MOUSE_DELTA  ; let [dx, dy] = gpu.mouse_delta()' },
  { name: 'OP_GPU_UPDATE_INPUT', value: '0x42', description: 'Update internal input state for this frame', category: 'gpu', example: 'OP_GPU_UPDATE_INPUT  ; Refresh input state' },
  { name: 'OP_GPU_BEGIN_COMMANDS', value: '0x43', description: 'Begin recording GPU command buffer', category: 'gpu', example: 'OP_GPU_BEGIN_COMMANDS  ; Start command recording' },
  { name: 'OP_GPU_END_COMMANDS', value: '0x44', description: 'Finish recording GPU command buffer', category: 'gpu', example: 'OP_GPU_END_COMMANDS  ; End command recording' },
  { name: 'OP_GPU_CMD_BEGIN_RP', value: '0x45', description: 'Begin render pass with color/depth attachments', category: 'gpu', example: 'OP_GPU_CMD_BEGIN_RP 0x0001  ; Begin render pass #1' },
  { name: 'OP_GPU_CMD_END_RP', value: '0x46', description: 'End current render pass', category: 'gpu', example: 'OP_GPU_CMD_END_RP  ; End render pass' },
  { name: 'OP_GPU_CMD_DRAW', value: '0x47', description: 'Draw primitive geometry (vertices, count)', category: 'gpu', example: 'OP_GPU_CMD_DRAW 0x0003 0x0006  ; Draw 6 vertices as triangles' },
  { name: 'OP_GPU_CMD_BIND_GP', value: '0x48', description: 'Bind graphics pipeline state object', category: 'gpu', example: 'OP_GPU_CMD_BIND_GP 0x0002  ; Bind pipeline #2' },
  { name: 'OP_GPU_CMD_BIND_DS', value: '0x49', description: 'Bind descriptor set (textures, uniforms)', category: 'gpu', example: 'OP_GPU_CMD_BIND_DS 0x0001 0x0000  ; Bind set 0 of layout 1' },
  { name: 'OP_GPU_CMD_SET_VP', value: '0x4A', description: 'Set viewport dimensions [x, y, w, h]', category: 'gpu', example: 'OP_GPU_CMD_SET_VP 0 0 800 600  ; viewport(0,0,800,600)' },
  { name: 'OP_GPU_CMD_SET_SC', value: '0x4B', description: 'Set scissor rectangle [x, y, w, h]', category: 'gpu', example: 'OP_GPU_CMD_SET_SC 0 0 800 600  ; scissor(0,0,800,600)' },
  { name: 'OP_GPU_CMD_BIND_VB', value: '0x4C', description: 'Bind vertex buffer at slot', category: 'gpu', example: 'OP_GPU_CMD_BIND_VB 0x0000 0x0001  ; Bind VB #1 to slot 0' },
  { name: 'OP_GPU_CMD_BIND_IB', value: '0x4D', description: 'Bind index buffer', category: 'gpu', example: 'OP_GPU_CMD_BIND_IB 0x0002  ; Bind index buffer #2' },
  { name: 'OP_GPU_CMD_DRAW_IDX', value: '0x4E', description: 'Draw indexed geometry (index count, offset)', category: 'gpu', example: 'OP_GPU_CMD_DRAW_IDX 0x000C 0x0000  ; Draw 12 indices from 0' },
  { name: 'OP_GPU_SUBMIT_SYNC', value: '0x4F', description: 'Submit commands with fence synchronization', category: 'gpu', example: 'OP_GPU_SUBMIT_SYNC 0x0001  ; Submit with fence #1' },
  { name: 'OP_GPU_ACQUIRE_IMG', value: '0x50', description: 'Acquire next swapchain image for rendering', category: 'gpu', example: 'OP_GPU_ACQUIRE_IMG  ; Acquire swapchain image' },
  { name: 'OP_GPU_PRESENT', value: '0x51', description: 'Present rendered image to screen', category: 'gpu', example: 'OP_GPU_PRESENT  ; Present frame' },
  { name: 'OP_GPU_WAIT_FENCE', value: '0x52', description: 'Wait for GPU fence to signal', category: 'gpu', example: 'OP_GPU_WAIT_FENCE 0x0001  ; Wait for fence #1' },
  { name: 'OP_GPU_RESET_FENCE', value: '0x53', description: 'Reset GPU fence for reuse', category: 'gpu', example: 'OP_GPU_RESET_FENCE 0x0001  ; Reset fence #1' },
  { name: 'OP_GPU_UPDATE_UNIFORM', value: '0x54', description: 'Update uniform buffer data by binding slot', category: 'gpu', example: 'OP_GPU_UPDATE_UNIFORM 0x0000  ; Update uniform slot 0' },
  { name: 'OP_GPU_CMD_PUSH_CONST', value: '0x55', description: 'Push constants to shader (small fast uniforms)', category: 'gpu', example: 'OP_GPU_CMD_PUSH_CONST 0x0000  ; Push constants to shader' },
  { name: 'OP_GPU_CMD_DISPATCH', value: '0x56', description: 'Dispatch compute shader [groups_x, groups_y, groups_z]', category: 'gpu', example: 'OP_GPU_CMD_DISPATCH 8 8 1  ; dispatch(8,8,1) compute' },

  // Math Extensions (0x57)
  { name: 'OP_MATH_PRINTM', value: '0x57', description: 'Print a matrix (array of arrays) in a formatted grid', category: 'math', example: 'OP_MATH_PRINTM  ; Print formatted [[1,2],[3,4]]' },

  // Halt (0xFF)
  { name: 'OP_HALT', value: '0xFF', description: 'Halt VM execution immediately', category: 'control', example: 'OP_HALT  ; Stop execution, return exit code' },

  // SRVM / RISC-V Operations (Custom/RV64I subset)
  { name: 'SRVM_ADD', value: '0x33', description: 'Add registers: rd = rs1 + rs2', category: 'srvm', example: 'SRVM_ADD x10, x11, x12 ; x10 = x11 + x12' },
  { name: 'SRVM_LDC', value: '0x5B', description: 'Load constant to register (U-type)', category: 'srvm', example: 'SRVM_LDC x10, 0x12345 ; x10 = 0x12345' },
  { name: 'SRVM_JAL', value: '0x6F', description: 'Jump and link (J-type)', category: 'srvm', example: 'SRVM_JAL x1, offset ; x1 = pc+4; pc += offset' },
]

export default function Opcodes() {
  const [activeCategory, setActiveCategory] = useState<Category>('all')
  const [searchQuery, setSearchQuery] = useState('')
  const [expandedOpcode, setExpandedOpcode] = useState<string | null>(null)
  const sectionRef = useRef<HTMLDivElement>(null)
  const gridRef = useRef<HTMLDivElement>(null)

  const filtered = opcodes.filter((op) => {
    const matchesCategory = activeCategory === 'all' || op.category === activeCategory
    const matchesSearch =
      op.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      op.description.toLowerCase().includes(searchQuery.toLowerCase()) ||
      op.example.toLowerCase().includes(searchQuery.toLowerCase())
    return matchesCategory && matchesSearch
  })

  useEffect(() => {
    if (!gridRef.current) return
    const cards = gridRef.current.querySelectorAll('.opcode-card')
    gsap.fromTo(
      cards,
      { opacity: 0, y: 20 },
      {
        opacity: 1,
        y: 0,
        duration: 0.4,
        stagger: 0.03,
        ease: 'power2.out',
      }
    )
  }, [activeCategory, searchQuery])

  const categoryColors: Record<string, string> = {
    stack: '#00f0ff',
    arithmetic: '#0aff60',
    comparison: '#a3a3ff',
    bitwise: '#ffaa00',
    control: '#ff6666',
    memory: '#66ccff',
    oop: '#cc66ff',
    exception: '#ff6699',
    gpu: '#ffcc00',
    math: '#00ffaa',
  }

  return (
    <section
      id="opcodes"
      ref={sectionRef}
      className="relative py-32 bg-void"
    >
      <div className="max-w-7xl mx-auto px-8">
        {/* Header */}
        <div className="text-center mb-16">
          <p className="font-mono text-sm text-sage-mid tracking-widest mb-4">
            INSTRUCTION SET
          </p>
          <h2 className="font-display text-4xl md:text-6xl font-bold text-white tracking-tight">
            Opcode Reference
          </h2>
          <p className="font-body text-white/50 mt-4 max-w-2xl mx-auto">
            61 general-purpose opcodes plus 28 GPU opcodes. Full parity with
            SageLang v3.6.5 MetalVM. Every opcode includes a usage example.
          </p>
        </div>

        {/* Search + Filters */}
        <div className="flex flex-col md:flex-row gap-4 mb-8">
          <div className="relative flex-1">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-white/40" />
            <input
              type="text"
              placeholder="Search opcodes, descriptions, or examples..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-10 pr-4 py-3 bg-surface border border-white/[0.08] rounded-lg font-mono text-sm text-white placeholder:text-white/30 focus:outline-none focus:border-sage-mid/50 transition-colors"
            />
          </div>
        </div>

        {/* Category tabs */}
        <div className="flex flex-wrap gap-2 mb-8">
          {categories.map((cat) => (
            <button
              key={cat.id}
              onClick={() => setActiveCategory(cat.id)}
              className={`px-4 py-2 rounded-lg font-mono text-xs transition-all duration-300 ${
                activeCategory === cat.id
                  ? 'bg-sage-mid/15 text-sage-mid border border-sage-mid/30'
                  : 'bg-white/[0.03] text-white/50 border border-transparent hover:border-white/[0.08] hover:text-white/80'
              }`}
            >
              {cat.label}
            </button>
          ))}
        </div>

        {/* Opcode grid */}
        <div
          ref={gridRef}
          className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3"
        >
          {filtered.map((op) => {
            const isExpanded = expandedOpcode === op.name
            const color = categoryColors[op.category] || '#00f0ff'

            return (
              <div
                key={op.name}
                id={op.name}
                className={`opcode-card glass-panel rounded-lg overflow-hidden transition-all duration-300 cursor-pointer ${
                  isExpanded ? 'border-sage-mid' : 'hover:border-white/[0.1]'
                }`}
                onClick={() =>
                  setExpandedOpcode(isExpanded ? null : op.name)
                }
              >
                <div className="p-4">
                  <div className="flex items-center justify-between mb-2">
                    <div className="flex items-center gap-3">
                      <Terminal
                        className="w-4 h-4 flex-shrink-0"
                        style={{ color }}
                      />
                      <span className="font-mono text-sm font-bold text-white">
                        {op.name}
                      </span>
                    </div>
                    <span
                      className="font-mono text-xs px-2 py-0.5 rounded"
                      style={{
                        background: `${color}15`,
                        color,
                      }}
                    >
                      {op.value}
                    </span>
                  </div>
                  <p className="text-white/50 text-xs leading-relaxed">
                    {op.description}
                  </p>

                  {/* Expanded content — always shows example now */}
                  {isExpanded && (
                    <div className="mt-3 pt-3 border-t border-white/[0.06]">
                      <p className="font-mono text-[10px] text-white/40 mb-1">
                        EXAMPLE:
                      </p>
                      <div className="code-block rounded p-2">
                        <code className="font-mono text-xs text-sage-light">
                          {op.example}
                        </code>
                      </div>
                    </div>
                  )}
                </div>
              </div>
            )
          })}
        </div>

        {filtered.length === 0 && (
          <div className="text-center py-16">
            <p className="text-white/40 font-mono text-sm">
              No opcodes match your search.
            </p>
          </div>
        )}

        {/* Stats */}
        <div className="flex justify-center gap-8 mt-12">
          <div className="text-center">
            <p className="font-display text-3xl font-bold text-sage-mid">
              61
            </p>
            <p className="font-mono text-xs text-white/40 mt-1">
              General Opcodes
            </p>
          </div>
          <div className="w-px bg-white/[0.08]" />
          <div className="text-center">
            <p className="font-display text-3xl font-bold text-sage-light">
              28
            </p>
            <p className="font-mono text-xs text-white/40 mt-1">GPU Opcodes</p>
          </div>
          <div className="w-px bg-white/[0.08]" />
          <div className="text-center">
            <p className="font-display text-3xl font-bold text-amber">
              89
            </p>
            <p className="font-mono text-xs text-white/40 mt-1">Total</p>
          </div>
          <div className="w-px bg-white/[0.08]" />
          <div className="text-center">
            <p className="font-display text-3xl font-bold text-sage-mid">
              89
            </p>
            <p className="font-mono text-xs text-white/40 mt-1">
              With Examples
            </p>
          </div>
        </div>
      </div>
    </section>
  )
}
