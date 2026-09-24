#include "l10n.h"

static const char *const messages[EIK_LANGUAGE_COUNT][EIK_MSG_COUNT] = {
    [EIK_LANGUAGE_ENGLISH] = {
        "Edgard in Kimeria", "Play", "About", "Options", "Exit", "Back", "Resume",
        "Exit to Menu", "Play Again", "Pause Menu", "Game Over", "Language",
        "Use WASD or Arrow Keys for movement.\nJ/Z to jump. K/X to attack. L/C to interact.",
        "Use the on-screen controls to move, jump, attack and interact.",
        "Arrows/Tab to move - Enter/Space/A to confirm - Esc/B to go back",
        "Edgard in Kimeria\n\nUse WASD or Arrow Keys for movement.\nJ/Z to jump. K/X to attack. L/C to interact.\nEscape to pause.\nCollect as many stars as you can and avoid enemies!",
        "Edgard in Kimeria\n\nUse the on-screen controls to move, jump, attack and interact.\nCollect as many stars as you can and avoid enemies!",
        "Lives", "Display: Fullscreen", "Display: Windowed",
    },
    [EIK_LANGUAGE_UKRAINIAN] = {
        "Едгард у Кімерії", "Грати", "Про гру", "Налаштування", "Вихід", "Назад",
        "Продовжити", "Вийти в меню", "Грати знову", "Меню паузи", "Гру закінчено",
        "Мова",
        "Використовуйте WASD або стрілки для руху.\nJ/Z — стрибок. K/X — атака. L/C — взаємодія.",
        "Використовуйте екранні елементи керування для руху, стрибка, атаки та взаємодії.",
        "Стрілки/Tab — рух - Enter/Пробіл/A — підтвердити - Esc/B — назад",
        "Едгард у Кімерії\n\nВикористовуйте WASD або стрілки для руху.\nJ/Z — стрибок. K/X — атака. L/C — взаємодія.\nEscape — пауза.\nЗберіть якомога більше зірок і уникайте ворогів!",
        "Едгард у Кімерії\n\nВикористовуйте екранні елементи керування для руху, стрибка, атаки та взаємодії.\nЗберіть якомога більше зірок і уникайте ворогів!",
        "Життя", "Екран: Повний", "Екран: У вікні",
    },
};

static const char *const language_names[EIK_LANGUAGE_COUNT] = {
    "English", "Українська",
};

const char *eik_l10n(EikLanguage language, EikMsg message)
{
    if (language >= EIK_LANGUAGE_COUNT || message >= EIK_MSG_COUNT) {
        return "";
    }
    return messages[language][message];
}

const char *eik_language_native_name(EikLanguage language)
{
    if (language >= EIK_LANGUAGE_COUNT) {
        return "";
    }
    return language_names[language];
}

size_t eik_l10n_message_count(void)
{
    return EIK_MSG_COUNT;
}
