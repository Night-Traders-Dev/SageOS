import { useEffect, useRef } from 'react'
import * as THREE from 'three'
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js'
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js'
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js'
import gsap from 'gsap'
import { ScrollTrigger } from 'gsap/ScrollTrigger'

gsap.registerPlugin(ScrollTrigger)

const GATE_COUNT = 120

function createGateGeometry(): THREE.BufferGeometry {
  const shape = new THREE.Shape()
  shape.moveTo(-0.15, -0.2)
  shape.lineTo(0.15, -0.2)
  shape.lineTo(0.15, 0)
  shape.lineTo(0, 0.1)
  shape.lineTo(-0.15, 0)
  shape.closePath()
  const extrudeSettings = { depth: 0.08, bevelEnabled: true, bevelThickness: 0.01, bevelSize: 0.01, bevelSegments: 2 }
  return new THREE.ExtrudeGeometry(shape, extrudeSettings)
}

function getGridPosition(index: number, total: number): THREE.Vector3 {
  const depth = Math.ceil(Math.cbrt(total))
  const width = Math.ceil(Math.sqrt(total / depth))
  const height = Math.ceil(total / (width * depth))
  const step = 0.8
  const layer = Math.floor(index / (width * height))
  const posInLayer = index % (width * height)
  const x = (posInLayer % width) * step - (width * step) / 2 + step / 2
  const y = Math.floor(posInLayer / width) * step - (height * step) / 2 + step / 2
  const z = layer * step - (depth * step) / 2 + step / 2
  return new THREE.Vector3(x, y, z)
}

export default function Hero() {
  const containerRef = useRef<HTMLDivElement>(null)
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const sceneRef = useRef<{
    renderer: THREE.WebGLRenderer
    scene: THREE.Scene
    camera: THREE.PerspectiveCamera
    composer: EffectComposer
    tumblerGroup: THREE.Group
    allGates: { mesh: THREE.Mesh; wireframeMesh: THREE.LineSegments; targetGridPos: THREE.Vector3; originRot: THREE.Vector3 }[]
    cleanup: () => void
  } | null>(null)

  useEffect(() => {
    const canvas = canvasRef.current
    const container = containerRef.current
    if (!canvas || !container) return

    const scene = new THREE.Scene()
    scene.fog = new THREE.FogExp2(0x030303, 0.08)

    const camera = new THREE.PerspectiveCamera(60, container.clientWidth / container.clientHeight, 0.1, 100)
    camera.position.set(0, 0, 8)

    const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false })
    renderer.setSize(container.clientWidth, container.clientHeight)
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))
    renderer.setClearColor(0x030303, 1)
    renderer.toneMapping = THREE.ReinhardToneMapping

    const renderScene = new RenderPass(scene, camera)
    
    // Balanced bloom attributes to prevent full-screen color burn out
    const bloomPass = new UnrealBloomPass(
      new THREE.Vector2(container.clientWidth, container.clientHeight),
      1.5, 0.4, 0.85
    )
    bloomPass.threshold = 0.2
    bloomPass.strength = 0.6
    bloomPass.radius = 0.6

    const composer = new EffectComposer(renderer)
    composer.addPass(renderScene)
    composer.addPass(bloomPass)

    scene.add(new THREE.AmbientLight(0xffffff, 0.05))
    const pLight1 = new THREE.PointLight(0x00f0ff, 2, 20)
    pLight1.position.set(5, 5, 5)
    scene.add(pLight1)
    const pLight2 = new THREE.PointLight(0x0aff60, 1.5, 20)
    pLight2.position.set(-5, -3, 3)
    scene.add(pLight2)

    const tumblerGroup = new THREE.Group()
    scene.add(tumblerGroup)

    const gateGeo = createGateGeometry()
    
    // Controlled material brightness to keep contrast high
    const gateMat = new THREE.MeshStandardMaterial({
      color: 0x001122,
      emissive: 0x00f0ff,
      emissiveIntensity: 0.8,
      metalness: 0.9,
      roughness: 0.1,
    })

    const allGates: { mesh: THREE.Mesh; wireframeMesh: THREE.LineSegments; targetGridPos: THREE.Vector3; originRot: THREE.Vector3 }[] = []

    for (let i = 0; i < GATE_COUNT; i++) {
      const mesh = new THREE.Mesh(gateGeo, gateMat.clone())
      mesh.position.set(
        (Math.random() - 0.5) * 6,
        (Math.random() - 0.5) * 6,
        (Math.random() - 0.5) * 6
      )
      const originRot = new THREE.Vector3(
        Math.random() * Math.PI * 2,
        Math.random() * Math.PI * 2,
        Math.random() * Math.PI * 2
      )
      mesh.rotation.set(originRot.x, originRot.y, originRot.z)
      mesh.scale.setScalar(0.5 + Math.random() * 0.8)
      tumblerGroup.add(mesh)

      const edges = new THREE.EdgesGeometry(gateGeo)
      const wireMat = new THREE.LineBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0 })
      const wireframeMesh = new THREE.LineSegments(edges, wireMat)
      mesh.add(wireframeMesh)

      allGates.push({
        mesh,
        wireframeMesh,
        targetGridPos: getGridPosition(i, GATE_COUNT),
        originRot,
      })
    }

    sceneRef.current = { renderer, scene, camera, composer, tumblerGroup, allGates, cleanup: () => {} }

    const scrollTrigger = ScrollTrigger.create({
      trigger: container,
      start: 'top top',
      end: 'bottom bottom',
      scrub: 1,
      onUpdate: (self) => {
        const progress = self.progress
        tumblerGroup.rotation.y += (progress * Math.PI * 4 - tumblerGroup.rotation.y) * 0.05
        tumblerGroup.rotation.x += (progress * Math.PI * 0.5 - tumblerGroup.rotation.x) * 0.05

        allGates.forEach((gate, idx) => {
          const itemProgress = idx / allGates.length
          if (progress > itemProgress) {
            const localProgress = Math.min(1, (progress - itemProgress) / (1 - itemProgress))
            const lerpFactor = localProgress * 0.08

            gate.mesh.position.lerp(gate.targetGridPos, lerpFactor)
            gate.mesh.rotation.x += (0 - gate.mesh.rotation.x) * lerpFactor
            gate.mesh.rotation.y += (0 - gate.mesh.rotation.y) * lerpFactor
            gate.mesh.rotation.z += (0 - gate.mesh.rotation.z) * lerpFactor

            const wireMat = gate.wireframeMesh.material as THREE.LineBasicMaterial
            wireMat.opacity = Math.max(0, (localProgress - 0.5) * 2)
          }
        })

        const targetZ = 8 - progress * 4
        camera.position.z += (targetZ - camera.position.z) * 0.05
      },
    })

    let animId: number
    const animate = () => {
      animId = requestAnimationFrame(animate)
      composer.render()
    }
    animate()

    const onResize = () => {
      if (!container) return
      camera.aspect = container.clientWidth / container.clientHeight
      camera.updateProjectionMatrix()
      renderer.setSize(container.clientWidth, container.clientHeight)
      composer.setSize(container.clientWidth, container.clientHeight)
    }
    window.addEventListener('resize', onResize)

    sceneRef.current.cleanup = () => {
      cancelAnimationFrame(animId)
      window.removeEventListener('resize', onResize)
      scrollTrigger.kill()
      gateGeo.dispose()
      gateMat.dispose()
      renderer.dispose()
      composer.dispose()
    }

    return () => {
      sceneRef.current?.cleanup()
    }
  }, [])

  return (
    <div ref={containerRef} className="hero-container">
      <div className="hero-sticky">
        <canvas ref={canvasRef} className="hero-canvas" />

        {/* Ambient bottom gradient shade isolates background objects and boosts content clarity */}
        <div className="absolute inset-0 z-10 flex flex-col justify-end p-6 md:p-16 pointer-events-none bg-gradient-to-t from-[#030303]/90 via-[#030303]/40 to-transparent">
          
          {/* Stack elements vertically on smaller ports to maintain clear component margins */}
          <div className="flex flex-col md:flex-row md:items-end md:justify-between gap-4 md:gap-6 pb-4">
            <div>
              {/* Scaled down min clamp bounds to prevent cross-component layout clashing */}
              <h1
                className="font-display font-bold text-white leading-tight tracking-tight"
                style={{
                  fontSize: 'clamp(44px, 10vw, 140px)',
                  textShadow: '0 0 20px rgba(0,240,255,0.15)',
                  letterSpacing: '-0.03em',
                }}
              >
                SageVM
              </h1>
              {/* Uplifted contrast value for readable body elements */}
              <p className="font-mono text-xs md:text-base text-white/80 mt-2 tracking-wider max-w-sm md:max-w-none">
                Self-hosted bytecode interpreter & compiler
              </p>
            </div>
            
            {/* Standardized simple node alignments across layouts */}
            <div className="text-left md:text-right mt-2 md:mt-0 border-t border-white/10 md:border-none pt-3 md:pt-0">
              <p className="font-mono text-[10px] md:text-xs text-white/60">
                Sage General Virtual Machine // v0.9.4
              </p>
              <div className="flex items-center gap-2 mt-1 justify-start md:justify-end">
                <span className="w-2 h-2 rounded-full bg-cyan-400 animate-pulse" />
                <span className="font-mono text-xs text-cyan-400 font-semibold">Active</span>
              </div>
            </div>
          </div>
        </div>

        <div className="absolute bottom-4 left-1/2 -translate-x-1/2 z-10 flex flex-col items-center gap-2">
          <div className="w-px h-8 bg-gradient-to-b from-transparent via-cyan-500/30 to-cyan-500 animate-pulse" />
        </div>
      </div>

      <style>{`
        .hero-container {
          position: relative;
          height: 300vh;
        }
        .hero-sticky {
          position: sticky;
          top: 0;
          height: 100vh;
          width: 100%;
          overflow: hidden;
          background: #030303;
        }
        .hero-canvas {
          position: absolute;
          top: 0;
          left: 0;
          width: 100%;
          height: 100%;
          z-index: 1;
        }
      `}</style>
    </div>
  )
}
