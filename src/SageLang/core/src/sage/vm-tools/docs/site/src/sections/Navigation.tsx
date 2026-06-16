import { useEffect, useState } from 'react'
import { Cpu } from 'lucide-react'

const navLinks = [
  { label: 'Architecture', href: '#architecture' },
  { label: 'Opcodes', href: '#opcodes' },
  { label: 'Playground', href: '#playground' },
]

export default function Navigation() {
  const [scrolled, setScrolled] = useState(false)

  useEffect(() => {
    const onScroll = () => setScrolled(window.scrollY > 80)
    window.addEventListener('scroll', onScroll, { passive: true })
    return () => window.removeEventListener('scroll', onScroll)
  }, [])

  const scrollTo = (href: string) => {
    const el = document.querySelector(href)
    if (el) el.scrollIntoView({ behavior: 'smooth' })
  }

  return (
    <nav
      className={`fixed top-4 left-1/2 -translate-x-1/2 z-50 transition-all duration-500 ${
        scrolled
          ? 'bg-surface/90 backdrop-blur-xl border border-white/[0.08]'
          : 'bg-white/[0.02] backdrop-blur-md border border-white/[0.04]'
      } rounded-full px-6 py-3`}
    >
      <div className="flex items-center gap-8">
        <button
          onClick={() => window.scrollTo({ top: 0, behavior: 'smooth' })}
          className="flex items-center gap-2 group"
        >
          <Cpu className="w-5 h-5 text-sage-mid group-hover:drop-shadow-[0_0_8px_rgba(0,240,255,0.6)] transition-all" />
          <span className="font-display font-bold text-white text-sm tracking-tight">
            SageVM
          </span>
        </button>

        <div className="hidden md:flex items-center gap-1">
          {navLinks.map((link) => (
            <button
              key={link.href}
              onClick={() => scrollTo(link.href)}
              className="px-4 py-1.5 text-sm text-white/60 hover:text-sage-mid transition-colors duration-300 rounded-full hover:bg-white/[0.04]"
            >
              {link.label}
            </button>
          ))}
        </div>

        <a
          href="https://github.com/Night-Traders-Dev/SageVM"
          target="_blank"
          rel="noopener noreferrer"
          className="hidden md:flex items-center gap-2 px-4 py-1.5 text-sm border border-sage-mid/30 text-sage-mid hover:bg-sage-mid/10 transition-all duration-300 rounded-full"
        >
          <svg className="w-4 h-4" fill="currentColor" viewBox="0 0 24 24">
            <path d="M12 0C5.37 0 0 5.37 0 12c0 5.31 3.435 9.795 8.205 11.385.6.105.825-.255.825-.57 0-.285-.015-1.23-.015-2.235-3.015.555-3.795-.735-4.035-1.41-.135-.345-.72-1.41-1.23-1.695-.42-.225-1.02-.78-.015-.795.945-.015 1.62.87 1.845 1.23 1.08 1.815 2.805 1.305 3.495.99.105-.78.42-1.305.765-1.605-2.67-.3-5.46-1.335-5.46-5.925 0-1.305.465-2.385 1.23-3.225-.12-.3-.54-1.53.12-3.18 0 0 1.005-.315 3.3 1.23.96-.27 1.98-.405 3-.405s2.04.135 3 .405c2.295-1.56 3.3-1.23 3.3-1.23.66 1.65.24 2.88.12 3.18.765.84 1.23 1.905 1.23 3.225 0 4.605-2.805 5.625-5.475 5.925.435.375.81 1.095.81 2.22 0 1.605-.015 2.895-.015 3.3 0 .315.225.69.825.57A12.02 12.02 0 0024 12c0-6.63-5.37-12-12-12z" />
          </svg>
          GitHub
        </a>
      </div>
    </nav>
  )
}
