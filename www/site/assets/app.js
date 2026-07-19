/**
 * C3 Landing Page — Interaction Controller
 * Scroll indicator, mouse-following background, navigation, animations
 */

(function () {
    'use strict';

    // ==================== 右侧滚动指示器 ====================
    const scrollDots = document.querySelectorAll('.scroll-dot');
    const sections = document.querySelectorAll('.section');

    function updateScrollIndicator() {
        const scrollY = window.scrollY;
        const windowHeight = window.innerHeight;
        let currentSection = 'hero';

        sections.forEach(section => {
            const rect = section.getBoundingClientRect();
            // Section considered "active" when its top is above 50% viewport
            if (rect.top <= windowHeight * 0.5) {
                currentSection = section.id;
            }
        });

        scrollDots.forEach(dot => {
            const section = dot.getAttribute('data-section');
            if (section === currentSection) {
                dot.classList.add('active');
            } else {
                dot.classList.remove('active');
            }
        });
    }

    // Throttle scroll handler
    let scrollTicking = false;
    window.addEventListener('scroll', () => {
        if (!scrollTicking) {
            requestAnimationFrame(() => {
                updateScrollIndicator();
                updateNavBackground();
                scrollTicking = false;
            });
            scrollTicking = true;
        }
    });

    // Smooth scroll for indicator clicks
    scrollDots.forEach(dot => {
        dot.addEventListener('click', e => {
            e.preventDefault();
            const targetId = dot.getAttribute('href').substring(1);
            const target = document.getElementById(targetId);
            if (target) {
                target.scrollIntoView({ behavior: 'smooth', block: 'start' });
            }
        });
    });

    // ==================== 顶部导航 ====================
    const topNav = document.getElementById('topNav');
    const navToggle = document.getElementById('navToggle');
    const navLinks = document.getElementById('navLinks');

    function updateNavBackground() {
        if (!topNav) return;
        if (window.scrollY > 40) {
            topNav.classList.add('scrolled');
        } else {
            topNav.classList.remove('scrolled');
        }
    }

    // Mobile nav toggle
    if (navToggle && navLinks) {
        navToggle.addEventListener('click', () => {
            navLinks.classList.toggle('open');
        });

        // Close on link click
        navLinks.querySelectorAll('a').forEach(link => {
            link.addEventListener('click', () => {
                navLinks.classList.remove('open');
            });
        });
    }

    // Smooth scroll for nav links
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', e => {
            e.preventDefault();
            const targetId = anchor.getAttribute('href').substring(1);
            const target = document.getElementById(targetId);
            if (target) {
                target.scrollIntoView({ behavior: 'smooth', block: 'start' });
            }
        });
    });

    // ==================== 滚动入场动画 ====================
    const animateElements = document.querySelectorAll('.animate-in');

    if ('IntersectionObserver' in window) {
        const observer = new IntersectionObserver((entries) => {
            entries.forEach(entry => {
                if (entry.isIntersecting) {
                    entry.target.style.animationPlayState = 'running';
                    observer.unobserve(entry.target);
                }
            });
        }, {
            threshold: 0.1,
            rootMargin: '0px 0px -50px 0px'
        });

        animateElements.forEach(el => {
            el.style.animationPlayState = 'paused';
            observer.observe(el);
        });
    } else {
        // Fallback: just show everything
        animateElements.forEach(el => {
            el.style.opacity = '1';
            el.style.animation = 'none';
        });
    }

    // ==================== 初始化 ====================
    updateScrollIndicator();
    updateNavBackground();

})();
