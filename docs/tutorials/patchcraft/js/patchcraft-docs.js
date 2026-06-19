/**
 * PatchCraft tutorial bundle — shared navigation & UI helpers
 */
(function () {
  'use strict';

  const sidebar = document.querySelector('.doc-sidebar');
  const menuToggle = document.querySelector('.menu-toggle');
  const currentPage = document.body.dataset.page || '';

  // Mobile sidebar toggle
  if (menuToggle && sidebar) {
    menuToggle.addEventListener('click', () => {
      sidebar.classList.toggle('open');
    });

    document.addEventListener('click', (e) => {
      if (!sidebar.classList.contains('open')) return;
      if (sidebar.contains(e.target) || menuToggle.contains(e.target)) return;
      sidebar.classList.remove('open');
    });
  }

  // Highlight active nav link
  document.querySelectorAll('.doc-nav a[data-page]').forEach((link) => {
    if (link.dataset.page === currentPage) {
      link.classList.add('active');
    }
  });

  // In-page TOC active state on scroll
  const tocLinks = document.querySelectorAll('.doc-toc a[href^="#"]');
  const sections = [];

  tocLinks.forEach((link) => {
    const id = link.getAttribute('href').slice(1);
    const el = document.getElementById(id);
    if (el) sections.push({ id, el, link });
  });

  if (sections.length > 0) {
    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (!entry.isIntersecting) return;
          tocLinks.forEach((l) => l.classList.remove('active'));
          const match = sections.find((s) => s.el === entry.target);
          if (match) match.link.classList.add('active');
        });
      },
      { rootMargin: '-20% 0px -70% 0px', threshold: 0 }
    );

    sections.forEach((s) => observer.observe(s.el));
  }

  // Copy buttons on code blocks
  document.querySelectorAll('.code-block').forEach((block) => {
    const btn = block.querySelector('.copy-btn');
    const pre = block.querySelector('pre');
    if (!btn || !pre) return;

    btn.addEventListener('click', async () => {
      const text = pre.textContent || '';
      try {
        await navigator.clipboard.writeText(text);
        const original = btn.textContent;
        btn.textContent = 'Copied';
        setTimeout(() => { btn.textContent = original; }, 1600);
      } catch {
        btn.textContent = 'Failed';
      }
    });
  });

  // Close mobile nav after in-page anchor click
  document.querySelectorAll('.doc-sidebar a[href^="#"]').forEach((link) => {
    link.addEventListener('click', () => {
      if (sidebar) sidebar.classList.remove('open');
    });
  });
})();
