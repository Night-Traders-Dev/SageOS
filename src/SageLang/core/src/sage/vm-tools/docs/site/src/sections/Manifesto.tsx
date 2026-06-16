import { useEffect, useRef } from 'react'
import gsap from 'gsap'
import { ScrollTrigger } from 'gsap/ScrollTrigger'

gsap.registerPlugin(ScrollTrigger)

const MANIFESTO_TEXT =
  'A self-hosted bytecode interpreter and compiler, bridging high-level SageLang abstractions directly to MetalVM\'s bare-metal execution substrate.'

export default function Manifesto() {
  const sectionRef = useRef<HTMLDivElement>(null)
  const textRef = useRef<HTMLParagraphElement>(null)

  useEffect(() => {
    const section = sectionRef.current
    const textEl = textRef.current
    if (!section || !textEl) return

    // Split into words and chars
    const words = MANIFESTO_TEXT.split(' ')
    textEl.innerHTML = words
      .map(
        (word) =>
          `<span class="word inline-block mr-[0.3em]">${word
            .split('')
            .map((char) => `<span class="char inline-block" style="transform-origin:50% 50% -20px;transform-style:preserve-3d">${char === ' ' ? '&nbsp;' : char}</span>`)
            .join('')}</span>`
      )
      .join('')

    const chars = textEl.querySelectorAll('.char')

    const tl = gsap.timeline({
      scrollTrigger: {
        trigger: section,
        start: 'top 70%',
        end: 'bottom 40%',
        scrub: 1,
      },
    })

    tl.fromTo(
      chars,
      {
        rotationY: -90,
        opacity: 0.2,
        scale: 0.8,
        color: '#ffffff',
      },
      {
        rotationY: 0,
        opacity: 1,
        scale: 1,
        color: '#00f0ff',
        ease: 'power2.out',
        stagger: 0.02,
      }
    )

    return () => {
      tl.kill()
    }
  }, [])

  return (
    <section
      ref={sectionRef}
      className="manifesto-section relative min-h-screen flex items-center justify-center bg-void"
      style={{ perspective: '1000px' }}
    >
      {/* Subtle background glow */}
      <div
        className="absolute inset-0 opacity-30 pointer-events-none"
        style={{
          background:
            'radial-gradient(ellipse at 50% 50%, rgba(0, 240, 255, 0.08) 0%, transparent 60%)',
        }}
      />

      <div className="relative z-10 max-w-5xl mx-auto px-8">
        <p
          ref={textRef}
          className="font-display text-3xl md:text-5xl lg:text-6xl font-bold leading-tight text-center"
          style={{ letterSpacing: '-0.02em' }}
        >
          {MANIFESTO_TEXT}
        </p>

        {/* Decorative elements */}
        <div className="flex justify-center gap-4 mt-16">
          <div className="w-16 h-px bg-gradient-to-r from-transparent to-sage-mid/50" />
          <div className="w-2 h-2 rotate-45 border border-sage-mid/50" />
          <div className="w-16 h-px bg-gradient-to-l from-transparent to-sage-mid/50" />
        </div>
      </div>
    </section>
  )
}
