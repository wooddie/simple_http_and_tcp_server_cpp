const translations = {
    kz: {
        main_title: "NeuroArt",
        subtitle: "Жасанды интеллект әлеміне қош келдіңіз",
        login_title: "Кіру",
        email_place: "Электрондық пошта",
        pass_place: "Құпия сөз",
        btn_login: "Жүйеге кіру",
        reg_title: "Тіркелу",
        iin_place: "ЖСН / ИИН (12 сан)",
        fio_place: "Толық аты-жөні (ФИО)",
        phone_place: "Телефон (+7...)",
        pass_reg_place: "Құпия сөз (8+ таңба)",
        btn_reg: "Тіркелу",
        loading: "Жүктеу...",
        reg_success: "Тіркелу сәтті аяқталды! Енді жүйеге кіріңіз.",
        conn_error: "Сервермен байланыс үзілді."
    },
    ru: {
        main_title: "NeuroArt",
        subtitle: "Добро пожаловать в мир ИИ",
        login_title: "Вход",
        email_place: "Электронная почта",
        pass_place: "Пароль",
        btn_login: "Войти в систему",
        reg_title: "Регистрация",
        iin_place: "ИИН (12 цифр)",
        fio_place: "ФИО полностью",
        phone_place: "Телефон (+7...)",
        pass_reg_place: "Пароль (8+ символов)",
        btn_reg: "Зарегистрироваться",
        loading: "Загрузка...",
        reg_success: "Регистрация прошла успешно! Теперь войдите.",
        conn_error: "Связь с сервером прервана."
    }
};

function setLanguage(lang) {
    localStorage.setItem('selectedLang', lang);
    document.querySelectorAll('[data-key]').forEach(el => {
        const key = el.getAttribute('data-key');
        const translation = translations[lang][key];
        if (translation) {
            if (el.tagName === 'INPUT') {
                el.placeholder = translation;
            } else {
                el.innerText = translation;
            }
        }
    });
}

async function handleForm(formId, endpoint, errorDivId) {
    document.getElementById(formId).onsubmit = async (e) => {
        e.preventDefault();
        const lang = localStorage.getItem('selectedLang') || 'kz';
        const formData = new URLSearchParams(new FormData(e.target));
        const errorDiv = document.getElementById(errorDivId);
        const btn = e.target.querySelector('button');

        errorDiv.innerText = "";
        btn.disabled = true;
        const originalText = btn.innerText;
        btn.innerText = translations[lang].loading;

        try {
            const response = await fetch(endpoint, {
                method: 'POST',
                body: formData
            });

            if (response.ok) {
                if (response.redirected) {
                    window.location.href = response.url;
                } else {
                    alert(translations[lang].reg_success);
                    e.target.reset();
                }
            } else {
                errorDiv.innerText = await response.text();
            }
        } catch (err) {
            errorDiv.innerText = translations[lang].conn_error;
        } finally {
            btn.disabled = false;
            btn.innerText = originalText;
        }
    };
}

// Инициализация
document.addEventListener('DOMContentLoaded', () => {
    const savedLang = localStorage.getItem('selectedLang') || 'kz';
    setLanguage(savedLang);
    handleForm('loginForm', '/login', 'loginError');
    handleForm('regForm', '/register', 'regError');
});