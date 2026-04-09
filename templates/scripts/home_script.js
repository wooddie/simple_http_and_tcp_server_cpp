const translations = {
    kz: {
        new_order: "Жаңа тапсырыс",
        type_label: "Контент түрі:",
        prompt_label: "Сипаттама (Промпт):",
        prompt_placeholder: "Мысалы: Көгілдір фондағы алтын бүркіт...",
        opt_img: "Сурет",
        opt_vid: "Бейне",
        btn_send: "Жіберу",
        projects_title: "Менің жобаларым",
        nav_create: "➕ Тапсырыс",
        nav_projects: "📂 Жобалар",
        nav_exit: "🚪 Шығу",
        status_pending: "Күйі: Күтілуде",
        status_approved: "Күйі: Мақұлданды",
        status_rejected: "Күйі: Қабылданбады"
    },
    ru: {
        new_order: "Новый заказ",
        type_label: "Тип контента:",
        prompt_label: "Описание (Промпт):",
        prompt_placeholder: "Например: Золотой орел на синем фоне...",
        opt_img: "Изображение",
        opt_vid: "Видео",
        btn_send: "Отправить",
        projects_title: "Мои проекты",
        nav_create: "➕ Создать",
        nav_projects: "📂 Проекты",
        nav_exit: "🚪 Выход",
        status_pending: "Статус: Ожидание",
        status_approved: "Статус: Принято",
        status_rejected: "Статус: Отклонено"
    }
};

function logout(event) {
    event.preventDefault();
    localStorage.clear();
    window.location.replace("/");
}

function showSection(id) {
    document.querySelectorAll('.section').forEach(s => s.classList.remove('visible'));
    document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));

    const target = document.getElementById(id);
    if (target) target.classList.add('visible');

    if (event && event.currentTarget) {
        event.currentTarget.classList.add('active');
    }
}

function setLanguage(lang) {
    localStorage.setItem('selectedLang', lang);

    document.querySelectorAll('[data-key]').forEach(el => {
        const key = el.getAttribute('data-key');
        const translation = translations[lang][key];
        if (translation) {
            if (el.tagName === 'TEXTAREA' || el.tagName === 'INPUT') {
                el.placeholder = translation;
            } else {
                el.innerText = translation;
            }
        }
    });

    document.querySelectorAll('.request-card').forEach(card => {
        const statusLabel = card.querySelector('.status-badge');
        if (statusLabel) {
            if (card.classList.contains('status-pending')) statusLabel.innerText = translations[lang].status_pending;
            if (card.classList.contains('status-approved')) statusLabel.innerText = translations[lang].status_approved;
            if (card.classList.contains('status-rejected')) statusLabel.innerText = translations[lang].status_rejected;
        }
    });
}

document.addEventListener('DOMContentLoaded', () => {
    const savedLang = localStorage.getItem('selectedLang') || 'kz';
    setLanguage(savedLang);
});

const themeBtn = document.getElementById('theme-toggle');

function changeTheme(btn) {
    document.body.classList.toggle('dark-mode');
    const isDark = document.body.classList.contains('dark-mode');
    localStorage.setItem('theme', isDark ? 'dark' : 'light');

    btn.textContent = isDark ? 'light' : 'dark';
}

if (localStorage.getItem('theme') === 'dark') {
    document.body.classList.add('dark-mode');
    if (themeBtn) themeBtn.textContent = 'light';
}
