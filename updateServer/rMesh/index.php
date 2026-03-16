<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>rMesh – Webflasher & Installer</title>

    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
    <script src="https://cdn.jsdelivr.net/npm/marked/marked.min.js"></script>

    <style>
        :root {
            --blue:      #0078ff;
            --blue-dark: #005fcc;
            --bg:        #f0f2f5;
            --card:      #ffffff;
            --text:      #1a1a2e;
            --muted:     #6b7280;
            --border:    #e5e7eb;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: var(--bg);
            color: var(--text);
            line-height: 1.6;
        }

        /* ── Header ────────────────────────────────── */
        header {
            background: #0f172a;
            color: white;
            padding: 0 24px;
            position: sticky;
            top: 0;
            z-index: 50;
            box-shadow: 0 2px 8px rgba(0,0,0,0.4);
        }

        .header-inner {
            max-width: 1100px;
            margin: 0 auto;
            display: flex;
            align-items: center;
            justify-content: space-between;
            height: 60px;
        }

        .header-logo {
            font-size: 22px;
            font-weight: 700;
            letter-spacing: 3px;
            color: #38bdf8;
        }

        .github-btn {
            display: flex;
            align-items: center;
            gap: 8px;
            background: rgba(255,255,255,0.1);
            color: white;
            text-decoration: none;
            padding: 8px 16px;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 500;
            transition: background 0.2s;
        }

        .github-btn:hover { background: rgba(255,255,255,0.2); }

        .github-btn svg {
            width: 20px;
            height: 20px;
            fill: white;
            flex-shrink: 0;
        }

        /* ── Hero ──────────────────────────────────── */
        .hero {
            background: linear-gradient(135deg, #0f172a 0%, #1e3a5f 100%);
            color: white;
            text-align: center;
            padding: 32px 24px;
        }

        .hero h1 {
            font-size: 36px;
            font-weight: 800;
            letter-spacing: 5px;
            color: #38bdf8;
            margin-bottom: 8px;
        }

        .hero p {
            font-size: 16px;
            color: #94a3b8;
            max-width: 620px;
            margin: 0 auto 20px;
        }

        .hero-links {
            display: flex;
            gap: 14px;
            justify-content: center;
            flex-wrap: wrap;
        }

        .hero-links a {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            padding: 10px 22px;
            border-radius: 8px;
            font-size: 15px;
            font-weight: 500;
            text-decoration: none;
            transition: opacity 0.2s, transform 0.2s;
        }

        .hero-links a:hover { opacity: 0.85; transform: translateY(-2px); }

        .btn-primary {
            background: var(--blue);
            color: white;
        }

        .btn-outline {
            background: rgba(255,255,255,0.1);
            color: white;
            border: 1px solid rgba(255,255,255,0.25);
        }

        .btn-outline svg { fill: white; }

        /* ── Main layout ───────────────────────────── */
        main {
            max-width: 1100px;
            margin: 0 auto;
            padding: 48px 24px;
        }

        .section-title {
            font-size: 22px;
            font-weight: 700;
            color: #0f172a;
            margin-bottom: 24px;
            padding-bottom: 10px;
            border-bottom: 3px solid var(--blue);
        }

        section { margin-bottom: 56px; }

        /* ── Device grid ───────────────────────────── */
        .device-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(270px, 1fr));
            gap: 24px;
        }

        .device-card {
            background: var(--card);
            border-radius: 16px;
            padding: 24px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.07);
            text-align: center;
            border: 1px solid var(--border);
            transition: transform 0.2s, box-shadow 0.2s;
        }

        .device-card:hover {
            transform: translateY(-4px);
            box-shadow: 0 10px 28px rgba(0,0,0,0.12);
        }

        .device-card img {
            width: 240px;
            height: 170px;
            object-fit: contain;
            border-radius: 10px;
            background: #f8fafc;
            border: 1px solid var(--border);
            margin-bottom: 16px;
        }

        .device-name {
            font-size: 16px;
            font-weight: 600;
            margin-bottom: 16px;
            color: #1e293b;
        }

        /* ── Install buttons ───────────────────────── */
        .btn-group {
            display: inline-flex;
            position: relative;
        }

        .btn-group button[slot="activate"] {
            border-radius: 8px 0 0 8px;
            background: var(--blue);
            color: white;
            border: none;
            padding: 10px 18px;
            cursor: pointer;
            font-size: 14px;
            font-weight: 500;
        }

        .btn-group button[slot="activate"]:hover { background: var(--blue-dark); }

        .btn-arrow {
            background: var(--blue-dark);
            color: white;
            border: none;
            border-left: 1px solid rgba(255,255,255,0.3);
            padding: 10px 12px;
            border-radius: 0 8px 8px 0;
            cursor: pointer;
            font-size: 13px;
            line-height: 1;
        }

        .btn-arrow:hover { background: #004baa; }

        .dropdown-menu {
            display: none;
            position: absolute;
            top: calc(100% + 4px);
            right: 0;
            background: white;
            border: 1px solid var(--border);
            border-radius: 8px;
            box-shadow: 0 8px 24px rgba(0,0,0,0.12);
            z-index: 100;
            min-width: 190px;
            text-align: left;
        }

        .dropdown-menu a {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 10px 15px;
            text-decoration: none;
            color: #333;
            font-size: 14px;
        }

        .dropdown-menu a:hover {
            background: #f8fafc;
            border-radius: 8px;
        }

        .btn-group.open .dropdown-menu { display: block; }

        button[slot="activate"].standalone {
            border-radius: 8px;
            background: var(--blue);
            color: white;
            border: none;
            padding: 10px 18px;
            cursor: pointer;
            font-size: 14px;
            font-weight: 500;
        }

        button[slot="activate"].standalone:hover { background: var(--blue-dark); }

        /* ── README / Markdown ─────────────────────── */
        .card {
            background: var(--card);
            border-radius: 16px;
            padding: 32px 36px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.07);
            border: 1px solid var(--border);
        }

        .readme-body h1 { display: none; } /* rMesh title already in hero */

        .readme-body h2 {
            font-size: 20px;
            color: #0f172a;
            margin: 28px 0 10px;
            padding-bottom: 6px;
            border-bottom: 1px solid var(--border);
        }

        .readme-body h3 {
            font-size: 17px;
            color: #1e293b;
            margin: 20px 0 8px;
        }

        .readme-body p { margin: 8px 0; color: #374151; }

        .readme-body ul, .readme-body ol {
            margin: 8px 0 8px 24px;
            color: #374151;
        }

        .readme-body li { margin: 4px 0; }

        .readme-body a { color: var(--blue); text-decoration: none; }
        .readme-body a:hover { text-decoration: underline; }

        .readme-body strong { color: #0f172a; }

        .readme-body code {
            background: #f1f5f9;
            padding: 2px 6px;
            border-radius: 4px;
            font-family: 'Consolas', monospace;
            font-size: 0.88em;
        }

        .readme-body pre {
            background: #0f172a;
            color: #e2e8f0;
            padding: 16px;
            border-radius: 10px;
            overflow-x: auto;
            margin: 14px 0;
        }

        .readme-body pre code {
            background: none;
            padding: 0;
            color: inherit;
            font-size: 0.9em;
        }

        .readme-loading {
            color: var(--muted);
            font-style: italic;
        }

        /* ── Changelog ─────────────────────────────── */
        .changelog-inner {
            background: #f8fafc;
            border: 1px solid var(--border);
            padding: 16px 20px;
            border-radius: 10px;
            margin-top: 0;
        }

        .changelog-inner pre {
            white-space: pre-wrap;
            word-wrap: break-word;
            margin: 0;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 13px;
            color: #374151;
            line-height: 1.65;
        }

        /* ── Footer ────────────────────────────────── */
        footer {
            background: #0f172a;
            color: #64748b;
            text-align: center;
            padding: 28px 24px;
            font-size: 14px;
        }

        footer a { color: #38bdf8; text-decoration: none; }
        footer a:hover { text-decoration: underline; }

        /* ── Responsive ────────────────────────────── */
        @media (max-width: 640px) {
            .hero h1 { font-size: 32px; }
            .card { padding: 20px; }
        }
    </style>
</head>
<body>

<!-- ── Header ── -->
<header>
    <div class="header-inner">
        <div class="header-logo">rMesh</div>
        <a href="https://github.com/dn9kgb/rmesh" target="_blank" rel="noopener" class="github-btn">
            <svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
                <path d="M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12"/>
            </svg>
            GitHub
        </a>
    </div>
</header>

<!-- ── Hero ── -->
<div class="hero">
    <h1>rMesh</h1>
    <p>Sicheres Textnachrichten-Mesh über LoRa – radikal minimalistisch, maximal effizient.</p>
    <div class="hero-links">
        <a href="#installer" class="btn-primary">Firmware installieren</a>
        <a href="https://github.com/dn9kgb/rmesh" target="_blank" rel="noopener" class="btn-outline">
            <svg viewBox="0 0 24 24" width="18" height="18" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
                <path d="M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12"/>
            </svg>
            Auf GitHub ansehen
        </a>
    </div>
</div>

<main>

    <!-- ── README from GitHub ── -->
    <section>
        <div class="card">
            <div id="readme-body" class="readme-body">
                <p class="readme-loading">Lade README von GitHub …</p>
            </div>
        </div>
    </section>

    <!-- ── Firmware installer ── -->
    <section id="installer">
        <h2 class="section-title">Firmware installieren</h2>
        <div class="device-grid">

        <?php
        $baseDir = __DIR__;
        $devices = array_filter(glob($baseDir . '/*'), 'is_dir');

        foreach ($devices as $devicePath) {
            $device = basename($devicePath);

            if (!file_exists("$devicePath/firmware.bin") && !file_exists("$devicePath/littlefs.bin")) {
                continue;
            }

            $imagePath = "$devicePath/image.webp";
            $imageUrl  = file_exists($imagePath)
                ? "$device/image.webp"
                : "https://via.placeholder.com/250x180?text=No+Image";
            $hasZip = file_exists("$devicePath/$device.zip");

            echo "<div class='device-card'>";
            echo "<img src='$imageUrl' alt='$device'>";
            echo "<div class='device-name'>$device</div>";

            if ($hasZip) {
                echo "
                <div class='btn-group'>
                    <esp-web-install-button manifest=\"$device/manifest.php\">
                        <button slot='activate'>Firmware installieren</button>
                        <span slot='unsupported'>Browser nicht unterstützt.</span>
                    </esp-web-install-button>
                    <button class='btn-arrow' onclick='toggleDropdown(this)'>&#9660;</button>
                    <div class='dropdown-menu'>
                        <a href='$device/$device.zip' download>&#8595; Firmware herunterladen</a>
                    </div>
                </div>
                ";
            } else {
                echo "
                <esp-web-install-button manifest=\"$device/manifest.php\">
                    <button slot='activate' class='standalone'>Firmware installieren</button>
                    <span slot='unsupported'>Browser nicht unterstützt.</span>
                </esp-web-install-button>
                ";
            }

            echo "</div>";
        }
        ?>

        </div>
    </section>

    <!-- ── Changelog ── -->
    <?php
    $globalChangelog = $baseDir . '/changelog.txt';
    if (file_exists($globalChangelog)):
        $content = htmlspecialchars(file_get_contents($globalChangelog));
    ?>
    <section>
        <h2 class="section-title">Release Notes / Changelog</h2>
        <div class="card">
            <div class="changelog-inner">
                <pre><?php echo $content; ?></pre>
            </div>
        </div>
    </section>
    <?php endif; ?>

</main>

<!-- ── Footer ── -->
<footer>
    <p>
        rMesh – Open Source unter <strong>GNU GPLv3</strong>
        &nbsp;|&nbsp;
        <a href="https://github.com/dn9kgb/rmesh" target="_blank" rel="noopener">github.com/dn9kgb/rmesh</a>
    </p>
</footer>

<script>
// Split-button dropdown
function toggleDropdown(btn) {
    const group = btn.closest('.btn-group');
    group.classList.toggle('open');
}

document.addEventListener('click', function(e) {
    if (!e.target.closest('.btn-group')) {
        document.querySelectorAll('.btn-group.open').forEach(g => g.classList.remove('open'));
    }
});

// Load README from GitHub and render as Markdown
fetch('https://raw.githubusercontent.com/dn9kgb/rmesh/main/README.md')
    .then(r => {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.text();
    })
    .then(md => {
        document.getElementById('readme-body').innerHTML = marked.parse(md);
    })
    .catch(() => {
        document.getElementById('readme-body').innerHTML =
            '<p style="color:var(--muted)">README konnte nicht von GitHub geladen werden.</p>';
    });
</script>

</body>
</html>
