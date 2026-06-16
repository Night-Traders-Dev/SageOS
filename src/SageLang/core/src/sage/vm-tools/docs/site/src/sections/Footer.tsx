import { Cpu, Github, BookOpen, Zap } from 'lucide-react'

const footerLinks = [
  {
    title: 'Project',
    links: [
      { label: 'GitHub', href: 'https://github.com/Night-Traders-Dev/SageVM', icon: Github },
      { label: 'Documentation', href: '#architecture', icon: BookOpen },
      { label: 'Issues', href: 'https://github.com/Night-Traders-Dev/SageVM/issues', icon: Zap },
    ],
  },
  {
    title: 'Resources',
    links: [
      { label: 'Architecture', href: '#architecture' },
      { label: 'Opcode Reference', href: '#opcodes' },
      { label: 'Playground', href: '#playground' },
    ],
  },
  {
    title: 'Ecosystem',
    links: [
      { label: 'SageLang', href: 'https://github.com/Night-Traders-Dev/SageLang' },
      { label: 'SageOS', href: '#' },
      { label: 'Night-Traders-Dev', href: 'https://github.com/Night-Traders-Dev' },
    ],
  },
]

export default function Footer() {
  const scrollTo = (href: string) => {
    if (href.startsWith('#')) {
      const el = document.querySelector(href)
      if (el) el.scrollIntoView({ behavior: 'smooth' })
    }
  }

  return (
    <footer className="relative bg-void border-t border-white/[0.06]">
      {/* Top glow line */}
      <div className="absolute top-0 left-0 right-0 h-px bg-gradient-to-r from-transparent via-sage-mid/30 to-transparent" />

      <div className="max-w-7xl mx-auto px-8 py-16">
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-5 gap-12">
          {/* Brand */}
          <div className="lg:col-span-2">
            <div className="flex items-center gap-3 mb-4">
              <Cpu className="w-6 h-6 text-sage-mid" />
              <span className="font-display font-bold text-xl text-white tracking-tight">
                SageVM
              </span>
            </div>
            <p className="text-white/50 text-sm leading-relaxed max-w-sm mb-6">
              A self-hosted bytecode interpreter and compiler for the SageLang
              ecosystem. Bridging high-level abstractions to bare-metal
              execution.
            </p>
            <div className="flex items-center gap-2">
              <span className="w-2 h-2 rounded-full bg-sage-light animate-pulse" />
              <span className="font-mono text-xs text-white/40">
               v0.9.4 — Active Development (JIT Phase 4)
              </span>

            </div>
          </div>

          {/* Links */}
          {footerLinks.map((group) => (
            <div key={group.title}>
              <h4 className="font-mono text-xs text-white/40 tracking-widest mb-4">
                {group.title.toUpperCase()}
              </h4>
              <ul className="space-y-3">
                {group.links.map((link) => (
                  <li key={link.label}>
                    {'href' in link && link.href.startsWith('http') ? (
                      <a
                        href={link.href}
                        target="_blank"
                        rel="noopener noreferrer"
                        className="text-white/60 text-sm hover:text-sage-mid transition-colors duration-300 inline-flex items-center gap-2 group"
                      >
                        {'icon' in link && link.icon && (
                          <link.icon className="w-3.5 h-3.5 text-white/30 group-hover:text-sage-mid transition-colors" />
                        )}
                        {link.label}
                      </a>
                    ) : (
                      <button
                        onClick={() => scrollTo(link.href)}
                        className="text-white/60 text-sm hover:text-sage-mid transition-colors duration-300"
                      >
                        {link.label}
                      </button>
                    )}
                  </li>
                ))}
              </ul>
            </div>
          ))}
        </div>

        {/* Bottom bar */}
        <div className="mt-16 pt-8 border-t border-white/[0.06] flex flex-col md:flex-row items-center justify-between gap-4">
          <p className="font-mono text-xs text-white/30">
            Built by Night-Traders-Dev. Powered by SageLang.
          </p>
          <div className="flex items-center gap-6">
            <a
              href="https://github.com/Night-Traders-Dev/SageVM"
              target="_blank"
              rel="noopener noreferrer"
              className="text-white/30 hover:text-sage-mid transition-colors"
            >
              <Github className="w-5 h-5" />
            </a>
            <span className="font-mono text-xs text-white/20">
              MIT License
            </span>
          </div>
        </div>
      </div>
    </footer>
  )
}
