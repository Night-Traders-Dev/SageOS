"use client"

import React, { useRef, useEffect, useState, useCallback } from "react"
import { motion, AnimatePresence } from "framer-motion"

interface Node {
  id: string
  label: string
  type: "core" | "compiler" | "vm" | "utils" | "bridge"
  x: number
  y: number
  vx: number
  vy: number
  radius: number
  description: string
  lines?: number
  imports?: string[]
}

interface Edge {
  source: string
  target: string
  label?: string
}

const INITIAL_NODES: Node[] = [
  {
    id: "sgvm_core",
    label: "sgvm_core.sage",
    type: "core",
    x: 400,
    y: 150,
    vx: 0,
    vy: 0,
    radius: 45,
    description: "Core utilities: SGVMUtils class, all 89 opcode constants, byte packing helpers",
    lines: 230,
    imports: []
  },
  {
    id: "sgvm_compiler",
    label: "sgvm_compiler.sage",
    type: "compiler",
    x: 200,
    y: 350,
    vx: 0,
    vy: 0,
    radius: 50,
    description: "Bytecode compiler: AST -> .sgvm binary, constant pool, chunk management",
    lines: 420,
    imports: ["sgvm_core"]
  },
  {
    id: "sgvm_vm",
    label: "sgvm_vm.sage",
    type: "vm",
    x: 600,
    y: 350,
    vx: 0,
    vy: 0,
    radius: 55,
    description: "MetalVM interpreter: stack machine, 89 opcode dispatch, delegation bridge",
    lines: 890,
    imports: ["sgvm_core", "math", "io", "net", "thread", "sys", "gpu", "ml_native"]
  },
  {
    id: "sgvm_entry",
    label: "sgvm.sage",
    type: "vm",
    x: 700,
    y: 550,
    vx: 0,
    vy: 0,
    radius: 35,
    description: "Entrypoint: CLI arg parsing, .sgvm file loading, shebang skip, debug flag",
    lines: 170,
    imports: ["sgvm_vm"]
  },
  {
    id: "sgvmc_entry",
    label: "sgvmc.sage",
    type: "compiler",
    x: 100,
    y: 550,
    vx: 0,
    vy: 0,
    radius: 35,
    description: "Compiler entrypoint: CLI args, --shebang flag, invokes SGVMCompiler",
    lines: 75,
    imports: ["sgvm_compiler"]
  },
  {
    id: "net",
    label: "net.sage",
    type: "bridge",
    x: 750,
    y: 200,
    vx: 0,
    vy: 0,
    radius: 30,
    description: "Network module: socket operations, TCP/UDP delegation",
    imports: []
  },
  {
    id: "gpu",
    label: "gpu.sage",
    type: "bridge",
    x: 650,
    y: 100,
    vx: 0,
    vy: 0,
    radius: 30,
    description: "GPU module: Vulkan/OpenGL command recording, 28 GPU opcodes",
    imports: []
  },
  {
    id: "math",
    label: "math",
    type: "bridge",
    x: 500,
    y: 50,
    vx: 0,
    vy: 0,
    radius: 28,
    description: "Host math module: sin, cos, tan, sqrt, pow, log, printm...",
    imports: []
  },
  {
    id: "thread",
    label: "thread",
    type: "bridge",
    x: 300,
    y: 50,
    vx: 0,
    vy: 0,
    radius: 28,
    description: "Host threading: mutex, lock/unlock, GIL for VM safety",
    imports: []
  },
]

const EDGES: Edge[] = [
  { source: "sgvm_core", target: "sgvm_compiler", label: "imports" },
  { source: "sgvm_core", target: "sgvm_vm", label: "imports" },
  { source: "sgvm_compiler", target: "sgvmc_entry", label: "used by" },
  { source: "sgvm_vm", target: "sgvm_entry", label: "used by" },
  { source: "sgvm_vm", target: "math", label: "delegates" },
  { source: "sgvm_vm", target: "net", label: "delegates" },
  { source: "sgvm_vm", target: "gpu", label: "delegates" },
  { source: "sgvm_vm", target: "thread", label: "delegates" },
]

const NODE_COLORS: Record<string, string> = {
  core: "#00f0ff",
  compiler: "#0aff60",
  vm: "#a3a3ff",
  utils: "#ffaa00",
  bridge: "#ff6666",
}

export default function DependencyGraph() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const [nodes, setNodes] = useState<Node[]>(INITIAL_NODES)
  const [hoveredNode, setHoveredNode] = useState<Node | null>(null)
  const [selectedNode, setSelectedNode] = useState<Node | null>(null)
  const [mousePos, setMousePos] = useState({ x: 0, y: 0 })
  const [scale, setScale] = useState(1)
  const [offset, setOffset] = useState({ x: 0, y: 0 })
  const [dragNode, setDragNode] = useState<string | null>(null)
  const [isPanning, setIsPanning] = useState(false)
  const [panStart, setPanStart] = useState({ x: 0, y: 0 })
  const [dimensions, setDimensions] = useState({ width: 800, height: 600 })
  const animationRef = useRef<number>(0)
  const lastTimeRef = useRef<number>(0)

  // Handle resizing and DPI
  useEffect(() => {
    const container = containerRef.current
    if (!container) return

    const observer = new ResizeObserver((entries) => {
      const entry = entries[0]
      if (entry) {
        const { width, height } = entry.contentRect
        setDimensions({ width, height })
      }
    })

    observer.observe(container)
    return () => observer.disconnect()
  }, [])

  // Force simulation step
  const simulate = useCallback((dt: number) => {
    setNodes(prev => {
      const newNodes = prev.map(n => ({ ...n }))
      const k = 0.05 * dt * 0.06 // spring constant
      const repulsion = 8000
      const centerForce = 0.0003

      // Repulsion between nodes
      for (let i = 0; i < newNodes.length; i++) {
        for (let j = i + 1; j < newNodes.length; j++) {
          const dx = newNodes[i].x - newNodes[j].x
          const dy = newNodes[i].y - newNodes[j].y
          const dist = Math.sqrt(dx * dx + dy * dy) || 1
          const force = repulsion / (dist * dist)
          const fx = (dx / dist) * force
          const fy = (dy / dist) * force
          newNodes[i].vx += fx
          newNodes[i].vy += fy
          newNodes[j].vx -= fx
          newNodes[j].vy -= fy
        }
      }

      // Spring attraction along edges
      EDGES.forEach(edge => {
        const src = newNodes.find(n => n.id === edge.source)
        const tgt = newNodes.find(n => n.id === edge.target)
        if (!src || !tgt) return
        const dx = tgt.x - src.x
        const dy = tgt.y - src.y
        const dist = Math.sqrt(dx * dx + dy * dy) || 1
        const idealDist = 180
        const force = (dist - idealDist) * k
        const fx = (dx / dist) * force
        const fy = (dy / dist) * force
        src.vx += fx
        src.vy += fy
        tgt.vx -= fx
        tgt.vy -= fy
      })

      // Center gravity
      const cx = dimensions.width / 2
      const cy = dimensions.height / 2
      newNodes.forEach(n => {
        n.vx += (cx - n.x) * centerForce
        n.vy += (cy - n.y) * centerForce
        // Damping
        n.vx *= 0.85
        n.vy *= 0.85
        // Update position (unless being dragged)
        if (dragNode !== n.id) {
          n.x += n.vx * dt * 0.06
          n.y += n.vy * dt * 0.06
        }
      })

      return newNodes
    })
  }, [dragNode, dimensions])

  // Render loop
  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext("2d")
    if (!ctx) return

    const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1
    canvas.width = dimensions.width * dpr
    canvas.height = dimensions.height * dpr
    
    const render = (time: number) => {
      const dt = Math.min((time - lastTimeRef.current) / 1000, 0.1) || 0.016
      lastTimeRef.current = time

      simulate(dt)

      ctx.clearRect(0, 0, canvas.width, canvas.height)
      ctx.save()
      ctx.scale(dpr, dpr)
      ctx.translate(offset.x, offset.y)
      ctx.scale(scale, scale)

      // Draw edges
      EDGES.forEach(edge => {
        const src = nodes.find(n => n.id === edge.source)
        const tgt = nodes.find(n => n.id === edge.target)
        if (!src || !tgt) return

        const isHighlighted = hoveredNode?.id === src.id || hoveredNode?.id === tgt.id ||
                             selectedNode?.id === src.id || selectedNode?.id === tgt.id

        ctx.beginPath()
        ctx.moveTo(src.x, src.y)
        ctx.lineTo(tgt.x, tgt.y)
        ctx.strokeStyle = isHighlighted ? "#00f0ff" : "rgba(100, 100, 120, 0.4)"
        ctx.lineWidth = isHighlighted ? 2.5 : 1.5
        ctx.stroke()

        // Edge label
        if (isHighlighted && edge.label) {
          const mx = (src.x + tgt.x) / 2
          const my = (src.y + tgt.y) / 2
          ctx.fillStyle = "#00f0ff"
          ctx.font = "10px monospace"
          ctx.textAlign = "center"
          ctx.fillText(edge.label, mx, my - 5)
        }
      })

      // Draw nodes
      nodes.forEach(node => {
        const isHovered = hoveredNode?.id === node.id
        const isSelected = selectedNode?.id === node.id
        const isConnected = hoveredNode ? EDGES.some(e =>
          (e.source === hoveredNode.id && e.target === node.id) ||
          (e.target === hoveredNode.id && e.source === node.id)
        ) : false

        const glow = isHovered || isSelected ? 20 : isConnected ? 10 : 0
        const color = NODE_COLORS[node.type]

        // Glow effect
        if (glow > 0) {
          const gradient = ctx.createRadialGradient(
            node.x, node.y, node.radius * 0.5,
            node.x, node.y, node.radius + glow
          )
          gradient.addColorStop(0, color + "40")
          gradient.addColorStop(1, "transparent")
          ctx.fillStyle = gradient
          ctx.beginPath()
          ctx.arc(node.x, node.y, node.radius + glow, 0, Math.PI * 2)
          ctx.fill()
        }

        // Node body
        ctx.beginPath()
        ctx.arc(node.x, node.y, node.radius, 0, Math.PI * 2)
        ctx.fillStyle = isHovered || isSelected ? color + "30" : "rgba(20, 20, 30, 0.9)"
        ctx.fill()
        ctx.strokeStyle = isHovered || isSelected ? color : "rgba(100, 100, 120, 0.6)"
        ctx.lineWidth = isHovered || isSelected ? 2.5 : 1.5
        ctx.stroke()

        // Inner fill
        ctx.beginPath()
        ctx.arc(node.x, node.y, node.radius * 0.85, 0, Math.PI * 2)
        ctx.fillStyle = isHovered || isSelected ? color + "15" : "rgba(15, 15, 25, 0.95)"
        ctx.fill()

        // Label
        ctx.fillStyle = isHovered || isSelected ? "#fff" : "rgba(200, 200, 220, 0.8)"
        ctx.font = isHovered || isSelected ? "bold 12px monospace" : "11px monospace"
        ctx.textAlign = "center"
        ctx.textBaseline = "middle"
        ctx.fillText(node.label, node.x, node.y)
      })

      ctx.restore()
      animationRef.current = requestAnimationFrame(render)
    }

    animationRef.current = requestAnimationFrame(render)
    return () => cancelAnimationFrame(animationRef.current)
  }, [nodes, hoveredNode, selectedNode, scale, offset, simulate, dimensions])

  // Coordinate extraction
  const getEventPos = (e: React.MouseEvent | React.TouchEvent) => {
    const canvas = canvasRef.current
    if (!canvas) return { x: 0, y: 0, clientX: 0, clientY: 0 }
    const rect = canvas.getBoundingClientRect()
    
    let clientX, clientY
    if ('touches' in e) {
      if (e.touches.length === 0) return { x: 0, y: 0, clientX: 0, clientY: 0 }
      clientX = e.touches[0].clientX
      clientY = e.touches[0].clientY
    } else {
      clientX = e.clientX
      clientY = e.clientY
    }

    return {
      x: (clientX - rect.left - offset.x) / scale,
      y: (clientY - rect.top - offset.y) / scale,
      clientX,
      clientY
    }
  }

  const handleStart = (e: React.MouseEvent | React.TouchEvent) => {
    const pos = getEventPos(e)
    const clicked = nodes.find(n => {
      const dx = pos.x - n.x
      const dy = pos.y - n.y
      return Math.sqrt(dx * dx + dy * dy) < n.radius
    })

    if (clicked) {
      setDragNode(clicked.id)
      setSelectedNode(clicked)
    } else {
      setIsPanning(true)
      setPanStart({ x: pos.clientX, y: pos.clientY })
      setSelectedNode(null)
    }
  }

  const handleMove = (e: React.MouseEvent | React.TouchEvent) => {
    const pos = getEventPos(e)
    setMousePos({ x: pos.clientX, y: pos.clientY })

    if (isPanning) {
      setOffset({
        x: offset.x + (pos.clientX - panStart.x),
        y: offset.y + (pos.clientY - panStart.y)
      })
      setPanStart({ x: pos.clientX, y: pos.clientY })
      return
    }

    if (dragNode) {
      setNodes(prev => prev.map(n =>
        n.id === dragNode ? { ...n, x: pos.x, y: pos.y, vx: 0, vy: 0 } : n
      ))
      return
    }

    // Hover detection (only for mouse)
    if (!('touches' in e)) {
      const hovered = nodes.find(n => {
        const dx = pos.x - n.x
        const dy = pos.y - n.y
        return Math.sqrt(dx * dx + dy * dy) < n.radius
      }) || null
      setHoveredNode(hovered)
    }
  }

  const handleEnd = () => {
    setDragNode(null)
    setIsPanning(false)
  }

  const handleWheel = (e: React.WheelEvent) => {
    e.preventDefault()
    const delta = e.deltaY > 0 ? 0.9 : 1.1
    const newScale = Math.max(0.3, Math.min(3, scale * delta))
    setScale(newScale)
  }

  const resetView = () => {
    setScale(1)
    setOffset({ x: 0, y: 0 })
    setNodes(INITIAL_NODES.map(n => ({ ...n })))
  }

  return (
    <div ref={containerRef} className="relative w-full h-[600px] md:h-[700px] bg-[#0a0a0f] rounded-xl overflow-hidden border border-white/[0.08] touch-none">
      {/* Controls */}
      <div className="absolute top-4 left-4 z-10 flex gap-2">
        <button
          onClick={resetView}
          className="px-3 py-1.5 bg-white/[0.05] hover:bg-white/[0.1] border border-white/[0.08] rounded-lg text-white/70 text-xs font-mono transition-colors"
        >
          Reset View
        </button>
        <div className="px-3 py-1.5 bg-white/[0.05] border border-white/[0.08] rounded-lg text-white/50 text-xs font-mono hidden md:block">
          Zoom: {(scale * 100).toFixed(0)}%
        </div>
      </div>

      {/* Legend */}
      <div className="absolute bottom-4 left-4 z-10 flex flex-col gap-1.5 pointer-events-none md:pointer-events-auto">
        {Object.entries(NODE_COLORS).map(([type, color]) => (
          <div key={type} className="flex items-center gap-2">
            <div className="w-2.5 h-2.5 rounded-full" style={{ backgroundColor: color }} />
            <span className="text-white/50 text-[10px] font-mono capitalize">{type}</span>
          </div>
        ))}
      </div>

      {/* Instructions */}
      <div className="absolute bottom-4 right-4 z-10 text-white/30 text-[10px] font-mono hidden md:block">
        Drag nodes • Scroll to zoom • Drag background to pan • Click node for details
      </div>

      <canvas
        ref={canvasRef}
        className="w-full h-full cursor-grab active:cursor-grabbing"
        onMouseMove={handleMove}
        onMouseDown={handleStart}
        onMouseUp={handleEnd}
        onMouseLeave={handleEnd}
        onWheel={handleWheel}
        onTouchStart={handleStart}
        onTouchMove={handleMove}
        onTouchEnd={handleEnd}
      />

      {/* Tooltip (Desktop Only) */}
      <AnimatePresence>
        {hoveredNode && !selectedNode && (
          <motion.div
            initial={{ opacity: 0, y: 5 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: 5 }}
            className="absolute z-20 pointer-events-none hidden md:block"
            style={{
              left: mousePos.x + 15,
              top: mousePos.y - 10,
            }}
          >
            <div className="bg-[#0f0f18] border border-white/[0.12] rounded-lg px-3 py-2 shadow-xl max-w-[240px]">
              <div className="flex items-center gap-2 mb-1">
                <div className="w-2 h-2 rounded-full" style={{ backgroundColor: NODE_COLORS[hoveredNode.type] }} />
                <span className="text-white font-mono text-xs font-bold">{hoveredNode.label}</span>
              </div>
              <p className="text-white/50 text-[10px] leading-relaxed">{hoveredNode.description}</p>
              {hoveredNode.lines && (
                <p className="text-white/30 text-[10px] mt-1">{hoveredNode.lines} lines</p>
              )}
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Detail Panel (Responsive) */}
      <AnimatePresence>
        {selectedNode && (
          <motion.div
            initial={{ opacity: 0, y: 20, x: 0 }}
            animate={{ opacity: 1, y: 0, x: 0 }}
            exit={{ opacity: 0, y: 20, x: 0 }}
            className="absolute bottom-0 md:bottom-auto md:top-4 right-0 md:right-4 z-20 w-full md:w-[320px] p-4 md:p-0"
          >
            <div className="bg-[#0f0f18]/95 backdrop-blur-md border border-white/[0.12] rounded-t-2xl md:rounded-xl p-4 md:p-5 shadow-2xl overflow-y-auto max-h-[60vh] md:max-h-none">
              <div className="flex items-center justify-between mb-4">
                <div className="flex items-center gap-2">
                  <div className="w-3 h-3 rounded-full" style={{ backgroundColor: NODE_COLORS[selectedNode.type] }} />
                  <span className="text-white font-mono text-sm md:text-base font-bold">{selectedNode.label}</span>
                </div>
                <button
                  onClick={() => setSelectedNode(null)}
                  className="text-white/40 hover:text-white text-lg md:text-xs p-1"
                >
                  ✕
                </button>
              </div>

              <p className="text-white/70 text-xs md:text-sm leading-relaxed mb-4">
                {selectedNode.description}
              </p>

              <div className="grid grid-cols-2 gap-4 mb-4">
                {selectedNode.lines && (
                  <div>
                    <span className="text-white/30 text-[10px] font-mono uppercase tracking-wider">Size</span>
                    <p className="text-white/90 text-xs font-mono">{selectedNode.lines} lines</p>
                  </div>
                )}
                <div>
                  <span className="text-white/30 text-[10px] font-mono uppercase tracking-wider">Type</span>
                  <p className="text-white/90 text-xs font-mono capitalize">{selectedNode.type}</p>
                </div>
              </div>

              {selectedNode.imports && selectedNode.imports.length > 0 && (
                <div className="mb-4">
                  <span className="text-white/30 text-[10px] font-mono uppercase tracking-wider">Imports</span>
                  <div className="flex flex-wrap gap-1.5 mt-1.5">
                    {selectedNode.imports.map(imp => (
                      <span key={imp} className="px-2 py-0.5 bg-white/[0.05] border border-white/[0.08] rounded text-white/60 text-[10px] font-mono">
                        {imp}
                      </span>
                    ))}
                  </div>
                </div>
              )}

              {/* Connected edges */}
              <div>
                <span className="text-white/30 text-[10px] font-mono uppercase tracking-wider">Connections</span>
                <div className="mt-2 space-y-2">
                  {EDGES.filter(e => e.source === selectedNode.id || e.target === selectedNode.id).map(edge => {
                    const otherId = edge.source === selectedNode.id ? edge.target : edge.source
                    const other = nodes.find(n => n.id === otherId)
                    if (!other) return null
                    return (
                      <div key={edge.source + edge.target} className="flex items-center gap-3 text-[10px] md:text-xs font-mono p-2 bg-white/[0.03] rounded-lg border border-white/[0.05]">
                        <span className="text-white/40 italic">{edge.label}</span>
                        <span className="text-white/90 font-bold">{other.label}</span>
                      </div>
                    )
                  })}
                </div>
              </div>
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  )
}
