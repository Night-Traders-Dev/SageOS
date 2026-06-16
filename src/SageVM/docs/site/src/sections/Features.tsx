import { useEffect, useRef } from 'react'
import gsap from 'gsap'
import { ScrollTrigger } from 'gsap/ScrollTrigger'
import { FileCode, GitBranch, Binary, Cpu } from 'lucide-react'

gsap.registerPlugin(ScrollTrigger)

const stages = [
  {
    num: '01',
    title: 'Source Code',
    subtitle: 'Lexical Analysis',
    description:
      'SageLang source files (.sage) are parsed into tokens by the frontend compiler. The lexer identifies identifiers, literals, operators, and language keywords.',
    icon: FileCode,
    color: '#00f0ff',
    code: `// hello.sage
proc main():
    print "Hello, SageVM!"
    var x = 42
    return x + 1`,
  },
  {
    num: '02',
    title: 'AST Generation',
    subtitle: 'IR Lowering',
    description:
      'The parser constructs an Abstract Syntax Tree representing the program structure. This AST is then lowered to SGIR (Sage General Intermediate Representation).',
    icon: GitBranch,
    color: '#a3a3ff',
    code: `AST:
  ProcDecl("main")
    StmtBlock
      Call("print", "Hello, SageVM!")
      VarDecl("x", IntLit(42))
      Return(Binary(Add, Id("x"), IntLit(1)))`,
  },
  {
    num: '03',
    title: 'Bytecode Emission',
    subtitle: 'SGVM Binary',
    description:
      'SGVMC (the compiler) traverses the IR and emits portable SGVM bytecode. Constants are packed into a pool, and instructions are serialized into chunks.',
    icon: Binary,
    color: '#0aff60',
    code: `OP_CONSTANT 0    ; "Hello, SageVM!"
OP_PRINT
OP_CONSTANT 1    ; 42
OP_DEFINE_GLOBAL 2  ; x
OP_GET_GLOBAL 2
OP_CONSTANT 3    ; 1
OP_ADD
OP_RETURN`,
  },
  {
    num: '04',
    title: 'JIT Compilation',
    subtitle: 'Native Target',
    description:
      'The register VM (SRVM) IR is JIT-compiled to native machine code (RISC-V/x86_64/ARM64) for high-performance execution. Includes OSR hooks and memory management.',
    icon: Cpu,
    color: '#ffaa00',
    code: `JIT Compile(SRVM_IR) -> 
Native RISC-V:
    ADD rd, rs1, rs2
    ADDI rd, rs1, imm
    JR ra
Status: Enabled (Hot-path OSR)`,
  },
]

export default function Features() {
  const sectionRef = useRef<HTMLDivElement>(null)
  const cardsRef = useRef<HTMLDivElement[]>([])

  useEffect(() => {
    const cards = cardsRef.current
    if (!cards.length) return

    cards.forEach((card, i) => {
      gsap.fromTo(
        card,
        { opacity: 0, y: 60, rotateX: 15 },
        {
          opacity: 1,
          y: 0,
          rotateX: 0,
          duration: 0.8,
          ease: 'power3.out',
          scrollTrigger: {
            trigger: card,
            start: 'top 85%',
            toggleActions: 'play none none reverse',
          },
          delay: i * 0.1,
        }
      )
    })

    return () => {
      ScrollTrigger.getAll().forEach((t) => t.kill())
    }
  }, [])

  return (
    <section
      ref={sectionRef}
      id="features"
      className="relative py-32 bg-void"
      style={{ perspective: '1200px' }}
    >
      {/* Background image with overlay */}
      <div className="absolute inset-0 opacity-20 pointer-events-none">
        <img
          src="/bytecode-flow.jpg"
          alt=""
          className="w-full h-full object-cover"
        />
        <div className="absolute inset-0 bg-gradient-to-b from-void via-void/80 to-void" />
      </div>

      <div className="relative z-10 max-w-7xl mx-auto px-8">
        {/* Section header */}
        <div className="text-center mb-20">
          <p className="font-mono text-sm text-sage-mid tracking-widest mb-4">
            THE PIPELINE
          </p>
          <h2 className="font-display text-4xl md:text-6xl font-bold text-white tracking-tight">
            Compilation Flow
          </h2>
          <p className="font-body text-white/50 mt-4 max-w-2xl mx-auto">
            From high-level SageLang source to bare-metal bytecode execution through
            a rigorously defined four-stage pipeline.
          </p>
        </div>

        {/* Pipeline stages */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {stages.map((stage, i) => {
            const Icon = stage.icon
            return (
              <div
                key={stage.num}
                ref={(el) => {
                  if (el) cardsRef.current[i] = el
                }}
                className="group relative"
                style={{ transformStyle: 'preserve-3d' }}
              >
                <div className="glass-panel rounded-xl p-6 md:p-8 transition-all duration-500 hover:border-sage-mid/20 h-full">
                  {/* Header */}
                  <div className="flex items-start justify-between mb-6">
                    <div className="flex items-center gap-4">
                      <div
                        className="w-12 h-12 rounded-lg flex items-center justify-center"
                        style={{
                          background: `${stage.color}15`,
                          border: `1px solid ${stage.color}30`,
                        }}
                      >
                        <Icon
                          className="w-6 h-6"
                          style={{ color: stage.color }}
                        />
                      </div>
                      <div>
                        <p
                          className="font-mono text-xs tracking-wider"
                          style={{ color: stage.color }}
                        >
                          STAGE {stage.num}
                        </p>
                        <h3 className="font-display text-xl font-bold text-white mt-0.5">
                          {stage.title}
                        </h3>
                      </div>
                    </div>
                  </div>

                  <p className="text-white/60 text-sm leading-relaxed mb-6">
                    {stage.description}
                  </p>

                  {/* Code block */}
                  <div className="code-block rounded-lg p-4 overflow-x-auto">
                    <pre className="font-mono text-xs">
                      <code className="text-white/80">{stage.code}</code>
                    </pre>
                  </div>

                  {/* Arrow connector (except last) */}
                  {i < stages.length - 1 && (
                    <div className="hidden lg:flex absolute -bottom-3 left-1/2 -translate-x-1/2 z-20">
                      <div className="w-6 h-6 rotate-45 bg-void border-r border-b border-sage-mid/30" />
                    </div>
                  )}
                </div>
              </div>
            )
          })}
        </div>

        {/* Execution arrow */}
        <div className="flex justify-center mt-12">
          <div className="flex items-center gap-4 font-mono text-xs text-white/40">
            <span>Source</span>
            <div className="flex items-center gap-1">
              <div className="w-8 h-px bg-white/20" />
              <div className="w-0 h-0 border-l-[5px] border-l-white/20 border-t-[3px] border-t-transparent border-b-[3px] border-b-transparent" />
            </div>
            <span className="text-sage-mid">AST</span>
            <div className="flex items-center gap-1">
              <div className="w-8 h-px bg-white/20" />
              <div className="w-0 h-0 border-l-[5px] border-l-white/20 border-t-[3px] border-t-transparent border-b-[3px] border-b-transparent" />
            </div>
            <span className="text-sage-light">Bytecode</span>
            <div className="flex items-center gap-1">
              <div className="w-8 h-px bg-white/20" />
              <div className="w-0 h-0 border-l-[5px] border-l-white/20 border-t-[3px] border-t-transparent border-b-[3px] border-b-transparent" />
            </div>
            <span className="text-amber">JIT Runtime</span>
          </div>
        </div>
      </div>
    </section>
  )
}
