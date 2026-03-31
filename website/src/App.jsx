import React, { useEffect } from 'react';
import './index.css';
import logoUrl from './assets/logo.png';
import heroUrl from './assets/hero.png';

function App() {
  
  // Basic scroll animation trigger
  useEffect(() => {
    const observer = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          entry.target.classList.add('animate-fade-up');
        }
      });
    }, { threshold: 0.1 });

    document.querySelectorAll('.animate-on-scroll').forEach((el) => {
      observer.observe(el);
    });

    return () => observer.disconnect();
  }, []);

  return (
    <div className="app-container">
      <header className="header">
        <div className="logo-section" onClick={() => window.scrollTo(0,0)}>
          <img src={logoUrl} alt="Alpha-Machine Logo" className="logo" />
          <h1>Alpha-Machine</h1>
        </div>
        <nav className="nav">
          <a href="#purpose">Purpose</a>
          <a href="#vision">Future Vision</a>
          <a href="#architecture">Tech Core</a>
          <a href="#status">Status</a>
          <a href="https://github.com/20obb/Alpha-Machine" target="_blank" rel="noreferrer" className="btn btn-github">GitHub</a>
        </nav>
      </header>

      <main>
        {/* HERO SECTION */}
        <section className="hero">
          <div className="hero-wrap">
            <div className="hero-content animate-fade-up">
              {/* Development Badge */}
              <span className="hero-pill delay-1 animate-fade-up" style={{ borderColor: '#ffa500', color: '#ff8c00', backgroundColor: 'rgba(255, 165, 0, 0.05)', animation: 'none' }}>
                🔧 PROJECT IS UNDER ACTIVE DEVELOPMENT
              </span>
              <h2 className="title delay-1 animate-fade-up">
                The Algorithmic <br/><span>Chess Engine</span> of Tomorrow.
              </h2>
              <p className="subtitle delay-2 animate-fade-up">
                Built from scratch in C++ for maximum computational speed. Alpha-Machine merges pure algorithmic logic with specialized data structures to conquer complex AI research tasks.
              </p>
              <div className="hero-actions delay-3 animate-fade-up">
                <a href="#purpose" className="btn btn-primary">Why We Built This?</a>
                <a href="#vision" className="btn btn-outline">See The Roadmap</a>
              </div>
            </div>
            
            <div className="hero-visual animate-fade-up delay-2">
              <div className="hero-blur-blob"></div>
              <div className="hero-image-wrapper">
                <img src={heroUrl} alt="Futuristic Chess King" className="hero-image" />
              </div>
            </div>
          </div>
        </section>

        {/* PURPOSE & UTILITY SECTION */}
        <section id="purpose" className="section bg-lightest">
          <div className="container animate-on-scroll" style={{opacity: 0}}>
            <span className="section-tag">Utility & Benefit</span>
            <h3 className="section-title">What is its Purpose?</h3>
            <p className="section-desc">
              Alpha-Machine is not just another chess player. It is a highly optimized computational backbone engineered to solve heavy algorithmic problems.
            </p>
            <div className="grid grid-2">
              <div className="glass-card">
                <div className="card-icon">🎯</div>
                <h4>High-Speed Analysis</h4>
                <p>The core benefit of Alpha-Machine is its ability to serve as an ultra-fast backend for chess analysis, quickly evaluating complex branch patterns for players and researchers without the overhead of heavy commercial engines.</p>
              </div>
              <div className="glass-card">
                <div className="card-icon">🧠</div>
                <h4>AI Testing Sandbox</h4>
                <p>By writing the foundation purely in C++ (without black-box neural networks initially), it grants researchers extreme control. It acts as the perfect testing ground for deploying new heuristic algorithms and programmatic bots.</p>
              </div>
            </div>
          </div>
        </section>

        {/* FUTURE VISION SECTION */}
        <section id="vision" className="section">
          <div className="container animate-on-scroll" style={{opacity: 0}}>
            <span className="section-tag">Roadmap</span>
            <h3 className="section-title">Future Vision & Evolution</h3>
            <p className="section-desc">
              The project is currently under heavy development. Our long-term goal is to evolve Alpha-Machine from a rigid algorithmic engine into a deeply learning, universally integrated AI entity.
            </p>
            <div className="grid grid-3">
              <div className="glass-card" style={{padding: '2rem'}}>
                <div className="card-icon" style={{width: '45px', height: '45px', fontSize: '1.2rem', marginBottom: '1rem'}}>1</div>
                <h4>Transposition Tables</h4>
                <p style={{fontSize: '0.95rem'}}>Implementing vast distributed memory caching. This will allow the engine to instantly recall evaluated branch patterns from millions of past nodes, unlocking grandmaster calculating depths.</p>
              </div>
              <div className="glass-card" style={{padding: '2rem'}}>
                <div className="card-icon" style={{width: '45px', height: '45px', fontSize: '1.2rem', marginBottom: '1rem'}}>2</div>
                <h4>UCI & GUI Integration</h4>
                <p style={{fontSize: '0.95rem'}}>Programming the Universal Chess Interface (UCI) protocol so Alpha-Machine can directly plug into leading graphical platforms like Lichess, Chess.com, and Arena automatically.</p>
              </div>
              <div className="glass-card" style={{padding: '2rem'}}>
                <div className="card-icon" style={{width: '45px', height: '45px', fontSize: '1.2rem', marginBottom: '1rem'}}>3</div>
                <h4>Self-Play Machine Learning</h4>
                <p style={{fontSize: '0.95rem'}}>The ultimate goal: replacing static values with NNUE architectures and having Alpha-Machine play millions of matches against itself to discover and learn strategy entirely on its own.</p>
              </div>
            </div>
          </div>
        </section>

        {/* ARCHITECTURE SECTION */}
        <section id="architecture" className="section bg-lightest">
          <div className="container animate-on-scroll" style={{opacity: 0}}>
            <span className="section-tag">Core Tech</span>
            <h3 className="section-title">Current Engine Architecture</h3>
            <p className="section-desc">
              The foundation driving the current version of the engine.
            </p>
            <div className="grid grid-2">
              <div className="glass-card" style={{padding: '2rem'}}>
                <div className="card-icon" style={{width: '45px', height: '45px', fontSize: '1.2rem', marginBottom: '1rem'}}>64</div>
                <h4>64-bit Bitboards & Magic Multipliers</h4>
                <p style={{fontSize: '0.95rem'}}>Leveraging continuous 64-bit unsigned integers to represent game states instantaneously via bitwise CPU instructions, complemented by O(1) collision hashing for sliding pieces.</p>
              </div>
              <div className="glass-card" style={{padding: '2rem'}}>
                <div className="card-icon" style={{width: '45px', height: '45px', fontSize: '1.2rem', marginBottom: '1rem'}}>🔍</div>
                <h4>Negamax Search</h4>
                <p style={{fontSize: '0.95rem'}}>Systematic depth-first evaluations empowered by Alpha-Beta pruning and dynamic quiescence horizon extensions to eliminate tactical blunders.</p>
              </div>
            </div>
          </div>
        </section>

        {/* STATUS SECTION */}
        <section id="status" className="section status-section">
          <div className="container text-center animate-on-scroll" style={{opacity: 0}}>
            <span className="section-tag">Logistics</span>
            <h3 className="section-title text-white">Current Engine Milestones</h3>
            <p className="section-desc" style={{color: 'rgba(255,255,255,0.7)'}}>
              As an actively developing project, rigorous Perft testing ensures absolute mathematical perfection.
            </p>
            <div className="stats-ticker">
              <div className="stat-box">
                <span className="stat-label">Max Velocity</span>
                <div className="stat-value">1.6M <span>NPS</span></div>
              </div>
              <div className="stat-box">
                <span className="stat-label">Logic Integrity</span>
                <div className="stat-value">100<span>%</span></div>
              </div>
              <div className="stat-box">
                <span className="stat-label">Current Release</span>
                <div className="stat-value">v0.3.0</div>
              </div>
            </div>
          </div>
        </section>
      </main>

      <footer className="footer">
        <div className="footer-nav">
          <a href="#purpose">Purpose</a>
          <a href="#vision">Future Vision</a>
          <a href="#architecture">Architecture</a>
          <a href="#status">Status</a>
        </div>
        <p>&copy; {new Date().getFullYear()} Alpha-Machine Chess Project. All rights reserved.</p>
        <p className="tagline">"Checkmate through pure calculation."</p>
      </footer>
    </div>
  );
}

export default App;
