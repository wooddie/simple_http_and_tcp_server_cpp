if (localStorage.getItem('theme') === 'dark') {
    document.body.classList.add('dark-mode');
}

function changeTheme(btn) {
    document.body.classList.toggle('dark-mode');
    const isDark = document.body.classList.contains('dark-mode');
    localStorage.setItem('theme', isDark ? 'dark' : 'light');

    if (btn) btn.textContent = isDark ? '☀️' : '🌙';
}

document.addEventListener('DOMContentLoaded', () => {
    const themeBtn = document.getElementById('theme-toggle');
    const isDark = document.body.classList.contains('dark-mode');

    if (themeBtn) {
        themeBtn.textContent = isDark ? '☀️' : '🌙';
    }
});
