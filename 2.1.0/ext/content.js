(() => {
  "use strict";

  const marker = "__webadMimoInstalled";
  if (!document.documentElement || window[marker]) {
    return;
  }
  Object.defineProperty(window, marker, { value: true });

  const host = document.createElement("div");
  host.setAttribute("aria-label", "Mimo, petit compagnon WebAD");
  host.style.cssText =
    "position:fixed!important;left:0;top:0;width:92px;height:112px;" +
    "z-index:2147483647!important;overflow:visible!important;" +
    "pointer-events:none!important;margin:0!important;padding:0!important;" +
    "border:0!important;background:transparent!important;";

  const shadow = host.attachShadow({ mode: "closed" });
  shadow.innerHTML = `
    <style>
      :host { all: initial; }
      * { box-sizing: border-box; }
      .pet {
        position: absolute;
        left: 8px;
        top: 5px;
        width: 76px;
        height: 100px;
        pointer-events: auto;
        cursor: grab;
        touch-action: none;
        user-select: none;
        -webkit-user-select: none;
        filter: drop-shadow(0 5px 5px #18214833);
      }
      .pet:active { cursor: grabbing; }
      .sprite {
        display: block;
        width: 76px;
        height: 88px;
        overflow: visible;
        transform-origin: 50% 90%;
        animation: mimo-bob 1.8s ease-in-out infinite;
      }
      .pet.walk .sprite { animation-duration: .42s; }
      .pet.paused .sprite { animation-play-state: paused; }
      .face { transform-origin: 50% 50%; animation: mimo-blink 5s infinite; }
      .bubble {
        position: absolute;
        left: 50%;
        bottom: 92px;
        max-width: 170px;
        padding: 7px 10px;
        border: 1px solid #d9d7ef;
        border-radius: 12px;
        background: #fff;
        color: #352f50;
        box-shadow: 0 4px 16px #18214820;
        font: 12px/1.35 system-ui, sans-serif;
        white-space: nowrap;
        opacity: 0;
        transform: translate(-50%, 5px) scale(.96);
        transition: opacity .18s ease, transform .18s ease;
        pointer-events: none;
      }
      .bubble.show { opacity: 1; transform: translate(-50%, 0) scale(1); }
      .tools {
        position: absolute;
        top: 0;
        right: -2px;
        display: flex;
        gap: 3px;
        opacity: 0;
        transition: opacity .16s ease;
      }
      .pet:hover .tools, .pet:focus-within .tools { opacity: 1; }
      button {
        display: grid;
        width: 23px;
        height: 23px;
        place-items: center;
        padding: 0;
        border: 1px solid #e4e1f2;
        border-radius: 50%;
        background: #fff;
        color: #51447e;
        font: 15px/1 system-ui, sans-serif;
        cursor: pointer;
        box-shadow: 0 2px 7px #18214820;
      }
      button:hover { background: #f2efff; }
      @keyframes mimo-bob {
        0%, 100% { transform: translateY(0) rotate(-2deg); }
        50% { transform: translateY(-5px) rotate(2deg); }
      }
      @keyframes mimo-blink {
        0%, 44%, 48%, 100% { transform: scaleY(1); }
        46% { transform: scaleY(.12); }
      }
      @media (prefers-reduced-motion: reduce) {
        .sprite, .face { animation: none !important; }
        .bubble, .tools { transition: none; }
      }
    </style>
    <div class="pet" role="group" aria-label="Mimo, petit compagnon">
      <div class="bubble" role="status" aria-live="polite"></div>
      <div class="tools">
        <button class="pause" type="button" aria-label="Mettre Mimo en pause" title="Pause">Ⅱ</button>
        <button class="hide" type="button" aria-label="Cacher Mimo sur cette page" title="Cacher">×</button>
      </div>
      <svg class="sprite" viewBox="0 0 76 88" role="img" aria-label="Petit chat violet et cyan">
        <defs>
          <linearGradient id="coat" x1="8" y1="8" x2="68" y2="80" gradientUnits="userSpaceOnUse">
            <stop stop-color="#43e3e5"/>
            <stop offset=".55" stop-color="#6599ff"/>
            <stop offset="1" stop-color="#9b69ef"/>
          </linearGradient>
          <linearGradient id="belly" x1="25" y1="42" x2="52" y2="77" gradientUnits="userSpaceOnUse">
            <stop stop-color="#fff"/>
            <stop offset="1" stop-color="#e9e4ff"/>
          </linearGradient>
        </defs>
        <path d="M18 57C4 55 5 36 15 34c7-1 7 8 2 10" fill="none"
          stroke="#735ee4" stroke-width="5" stroke-linecap="round"/>
        <path d="M17 31 14 12l17 11M44 23l17-11-3 20"
          fill="url(#coat)" stroke="#5148bb" stroke-width="2.5"
          stroke-linejoin="round"/>
        <path d="M16 34c0-13 9-21 22-21s22 8 22 21v21c0 15-9 25-22 25S16 70 16 55Z"
          fill="url(#coat)" stroke="#5148bb" stroke-width="2.5"/>
        <path d="M25 57c0-8 5-13 13-13s13 5 13 13v10c0 6-5 10-13 10s-13-4-13-10Z"
          fill="url(#belly)" opacity=".96"/>
        <g class="face" fill="#302c5b">
          <ellipse cx="29" cy="40" rx="2.3" ry="3.4"/>
          <ellipse cx="47" cy="40" rx="2.3" ry="3.4"/>
          <path d="M36 45q2-2 4 0-2 3-4 0"/>
          <path d="M38 48q-2 3-4 0m4 0q2 3 4 0" fill="none"
            stroke="#5148bb" stroke-width="1.5" stroke-linecap="round"/>
        </g>
        <circle cx="22" cy="47" r="2.6" fill="#ff91b7" opacity=".8"/>
        <circle cx="54" cy="47" r="2.6" fill="#ff91b7" opacity=".8"/>
        <path d="M28 80v3m20-3v3" stroke="#5148bb" stroke-width="3" stroke-linecap="round"/>
        <path d="M27 10q-2-5 2-8m20 8q2-5-2-8" fill="none"
          stroke="#43cdda" stroke-width="2.4" stroke-linecap="round"/>
      </svg>
    </div>`;

  document.documentElement.appendChild(host);

  const pet = shadow.querySelector(".pet");
  const bubble = shadow.querySelector(".bubble");
  const pauseButton = shadow.querySelector(".pause");
  const hideButton = shadow.querySelector(".hide");
  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  const messages = [
    "Coucou ! Je m’appelle Mimo.",
    "Tu peux me déplacer partout !",
    "Petite pause avant la prochaine page ?",
    "Je garde un œil sur ton onglet.",
    "Miaou ✨"
  ];
  let x = Math.max(8, window.innerWidth - 112);
  let y = Math.max(8, window.innerHeight - 128);
  let targetX = x;
  let targetY = y;
  let lastMove = performance.now();
  let nextWander = lastMove + 1800;
  let lastFrame = lastMove;
  let bubbleTimer = 0;
  let dragging = false;
  let moved = false;
  let dragOffsetX = 0;
  let dragOffsetY = 0;
  let paused = false;
  let facingLeft = false;

  const clampPosition = () => {
    const maxX = Math.max(0, window.innerWidth - host.offsetWidth);
    const maxY = Math.max(0, window.innerHeight - host.offsetHeight);
    x = Math.max(0, Math.min(maxX, x));
    y = Math.max(0, Math.min(maxY, y));
    targetX = Math.max(0, Math.min(maxX, targetX));
    targetY = Math.max(0, Math.min(maxY, targetY));
  };

  const place = () => {
    host.style.left = `${Math.round(x)}px`;
    host.style.top = `${Math.round(y)}px`;
    pet.style.transform = facingLeft ? "scaleX(-1)" : "scaleX(1)";
  };

  const say = (message) => {
    bubble.textContent = message;
    bubble.classList.add("show");
    window.clearTimeout(bubbleTimer);
    bubbleTimer = window.setTimeout(() => bubble.classList.remove("show"), 2400);
  };

  const chooseDestination = (now) => {
    const maxX = Math.max(0, window.innerWidth - host.offsetWidth);
    const maxY = Math.max(0, window.innerHeight - host.offsetHeight);
    targetX = Math.random() * maxX;
    targetY = Math.random() * maxY;
    facingLeft = targetX < x;
    pet.classList.add("walk");
    nextWander = now + 1800 + Math.random() * 2800;
  };

  const animate = (now) => {
    if (!host.isConnected) {
      return;
    }
    const elapsed = Math.min((now - lastFrame) / 1000, .05);
    lastFrame = now;

    if (!paused && !dragging && !reduceMotion.matches) {
      if (now >= nextWander) {
        chooseDestination(now);
      }
      const dx = targetX - x;
      const dy = targetY - y;
      const distance = Math.hypot(dx, dy);
      if (distance < 3) {
        x = targetX;
        y = targetY;
        pet.classList.remove("walk");
      } else {
        const step = Math.min(distance, elapsed * 38);
        x += (dx / distance) * step;
        y += (dy / distance) * step;
      }
      place();
    }
    requestAnimationFrame(animate);
  };

  pet.addEventListener("pointerdown", (event) => {
    if (event.target instanceof HTMLButtonElement) {
      return;
    }
    dragging = true;
    moved = false;
    dragOffsetX = event.clientX - x;
    dragOffsetY = event.clientY - y;
    pet.setPointerCapture(event.pointerId);
    event.preventDefault();
  });

  pet.addEventListener("pointermove", (event) => {
    if (!dragging) {
      return;
    }
    moved ||= Math.hypot(event.clientX - (x + dragOffsetX),
      event.clientY - (y + dragOffsetY)) > 3;
    x = event.clientX - dragOffsetX;
    y = event.clientY - dragOffsetY;
    targetX = x;
    targetY = y;
    clampPosition();
    place();
  });

  const stopDragging = () => {
    dragging = false;
    pet.classList.remove("walk");
  };
  pet.addEventListener("pointerup", stopDragging);
  pet.addEventListener("pointercancel", stopDragging);
  pet.addEventListener("click", () => {
    if (!moved) {
      say(messages[Math.floor(Math.random() * messages.length)]);
    }
  });

  pauseButton.addEventListener("click", () => {
    paused = !paused;
    pet.classList.toggle("paused", paused);
    pauseButton.textContent = paused ? "▶" : "Ⅱ";
    pauseButton.setAttribute("aria-label", paused ? "Reprendre Mimo" : "Mettre Mimo en pause");
    pauseButton.title = paused ? "Reprendre" : "Pause";
    if (!paused) {
      nextWander = performance.now() + 900;
    }
  });

  hideButton.addEventListener("click", () => host.remove());
  window.addEventListener("resize", () => {
    clampPosition();
    place();
  }, { passive: true });
  reduceMotion.addEventListener?.("change", (event) => {
    if (event.matches) {
      pet.classList.remove("walk");
    } else {
      nextWander = performance.now() + 900;
    }
  });

  place();
  requestAnimationFrame(animate);
  window.setTimeout(() => say("Coucou ! Je m’appelle Mimo."), 900);
})();
