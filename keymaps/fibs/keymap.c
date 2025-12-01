#include QMK_KEYBOARD_H
#include "secrets.h"

enum layers {
    WIN_BASE,
    WIN_HRM,
    WIN_NAV,
    WIN_GAME,
    WIN_FN,
};

enum wasd_keys {
    WASD_NONE = 0,
    WASD_W = 1,
    WASD_A = 2,
    WASD_S = 3,
    WASD_D = 4,
};

enum custom_keycodes {
    SOCD_W = SAFE_RANGE,
    SOCD_A,
    SOCD_S,
    SOCD_D,
    PASS,
};


#define MS_UP    KC_MS_UP
#define MS_LEFT  KC_MS_LEFT
#define MS_DOWN  KC_MS_DOWN
#define MS_RIGHT KC_MS_RIGHT
#define MS_BTN1  KC_MS_BTN1
#define MS_BTN2  KC_MS_BTN2
#define MS_BTN3  KC_MS_BTN3
#define MS_WHLU  KC_MS_WH_UP
#define MS_WHLD  KC_MS_WH_DOWN

// SOCD state: what keys are physically held and what should be sent
static bool w_held = false;
static bool a_held = false;
static bool s_held = false;
static bool d_held = false;

static bool socd_w_active = false;
static bool socd_a_active = false;
static bool socd_s_active = false;
static bool socd_d_active = false;

typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t mod;
    bool engaged;
} mod_registry_t;

#define MAX_MOD_REGISTRY 8
static mod_registry_t mod_registry[MAX_MOD_REGISTRY];

static layer_state_t saved_layers = 0;
static bool          game_mode    = false;
static uint8_t       wasd_keys_pressed = 0;
static uint8_t       wasd_alternating_count = 0;
static uint8_t       last_wasd_key = WASD_NONE;

#define KC_TASK LGUI(KC_TAB)
#define KC_FLXP LGUI(KC_E)

#define HM_A   MT(MOD_LGUI, KC_A)
#define HM_S   MT(MOD_LALT, KC_S)
#define HM_D   MT(MOD_LCTL, KC_D)
#define HM_F   MT(MOD_LSFT, KC_F)

#define HM_J   MT(MOD_RSFT, KC_J)
#define HM_K   MT(MOD_RCTL, KC_K)
#define HM_L   MT(MOD_RALT, KC_L)
#define HM_SCL MT(MOD_RGUI, KC_SCLN)


// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [WIN_BASE] = LAYOUT_tkl_ansi(
        KC_ESC,             KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,     KC_F11,   KC_F12,   KC_PSCR,  KC_SCRL,  KC_PAUS,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,    KC_EQL,   KC_BSPC,  KC_INS,   KC_HOME,  KC_PGUP,
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,    KC_RBRC,  KC_BSLS,  KC_DEL,   KC_END,   KC_PGDN,
        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,              KC_ENT,
        KC_LSFT,            KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,              KC_RSFT,            KC_UP,
        KC_LCTL,  KC_LWIN,  KC_LALT,                                KC_SPC,                                 KC_RALT,  MO(WIN_FN), KC_NO,    KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT),

    [WIN_HRM] = LAYOUT_tkl_ansi(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  HM_A,     HM_S,     HM_D,     HM_F,     _______,  _______,  HM_J,     HM_K,     HM_L,     HM_SCL,   _______,              _______,                                
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  _______,  MO(WIN_NAV),                            _______,                                _______,  _______,    _______,  _______,  _______,  _______,  _______),

    [WIN_NAV] = LAYOUT_tkl_ansi(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  PASS,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  MS_BTN3,  MS_WHLU,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  MS_BTN1,  MS_BTN2,  MS_WHLD,
        _______,  KC_LGUI,  KC_LALT,  KC_LCTL,  KC_LSFT,  _______,  KC_LEFT,  KC_DOWN,  KC_UP,    KC_RGHT,  _______,  _______,              _______,                                
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,            MS_UP,
        _______,  _______,  _______,                                _______,                                _______,  _______,    _______,  _______,  MS_LEFT,  MS_DOWN,  MS_RIGHT),

    [WIN_GAME] = LAYOUT_tkl_ansi(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  SOCD_W,   _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  SOCD_A,   SOCD_S,   SOCD_D,   _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,    _______,  _______,  _______,  _______,  _______),

    [WIN_FN] = LAYOUT_tkl_ansi(
        _______,            KC_BRID,  KC_BRIU,  KC_TASK,  KC_FLXP,  BL_DOWN,  BL_UP,    KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,    KC_VOLD,  KC_VOLU,  KC_PSCR,  _______,  BL_STEP,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        BL_TOGG,  BL_STEP,  BL_UP,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  BL_DOWN,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,
        _______,            _______,  _______,  _______,  _______,  _______,  NK_TOGG,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  GU_TOGG,  _______,                                _______,                                _______,  _______,    _______,  _______,  _______,  _______,  _______)
};
// clang-format on


void keyboard_post_init_user(void) {
    layer_on(WIN_HRM);
}

static uint8_t get_wasd_key(uint8_t row, uint8_t col) {
    if (row == 2 && col == 2) return WASD_W;
    if (row == 3 && col == 1) return WASD_A;
    if (row == 3 && col == 2) return WASD_S;
    if (row == 3 && col == 3) return WASD_D;
    return WASD_NONE;
}

static void reset_wasd_tracking(void) {
    wasd_keys_pressed = 0;
    wasd_alternating_count = 0;
    last_wasd_key = WASD_NONE;
}

static void register_mod_at_position(uint8_t row, uint8_t col, uint8_t mod) {
    for (int i = 0; i < MAX_MOD_REGISTRY; i++) {
        if (!mod_registry[i].engaged) {
            mod_registry[i].row = row;
            mod_registry[i].col = col;
            mod_registry[i].mod = mod;
            mod_registry[i].engaged = true;
            return;
        }
    }
}

static void unregister_mod_at_position(uint8_t row, uint8_t col) {
    for (int i = 0; i < MAX_MOD_REGISTRY; i++) {
        if (mod_registry[i].engaged && mod_registry[i].row == row && mod_registry[i].col == col) {
            unregister_mods(mod_registry[i].mod);
            mod_registry[i].engaged = false;
            return;
        }
    }
}

// SOCD logic: track physical keys and compute which should be active
static uint8_t last_vertical_key = WASD_NONE;
static uint8_t last_horizontal_key = WASD_NONE;

static void update_socd_state(uint8_t wasd_key, bool pressed) {
    if (!wasd_key) return;

    // Update physical state
    switch (wasd_key) {
        case WASD_W:
            w_held = pressed;
            if (pressed) last_vertical_key = WASD_W;
            break;
        case WASD_A:
            a_held = pressed;
            if (pressed) last_horizontal_key = WASD_A;
            break;
        case WASD_S:
            s_held = pressed;
            if (pressed) last_vertical_key = WASD_S;
            break;
        case WASD_D:
            d_held = pressed;
            if (pressed) last_horizontal_key = WASD_D;
            break;
    }

    if (w_held && !s_held) {
        socd_w_active = true;
        socd_s_active = false;
    } else if (s_held && !w_held) {
        socd_s_active = true;
        socd_w_active = false;
    } else if (w_held && s_held) {
        if (last_vertical_key == WASD_W) {
            socd_w_active = true;
            socd_s_active = false;
        } else {
            socd_s_active = true;
            socd_w_active = false;
        }
    } else {
        socd_w_active = false;
        socd_s_active = false;
    }

    if (a_held && !d_held) {
        socd_a_active = true;
        socd_d_active = false;
    } else if (d_held && !a_held) {
        socd_d_active = true;
        socd_a_active = false;
    } else if (a_held && d_held) {
        // Both held - last input wins
        if (last_horizontal_key == WASD_A) {
            socd_a_active = true;
            socd_d_active = false;
        } else {
            socd_d_active = true;
            socd_a_active = false;
        }
    } else {
        socd_a_active = false;
        socd_d_active = false;
    }
}

static void apply_socd_to_os(void) {
    if (socd_w_active) {
        register_code(KC_W);
    } else {
        unregister_code(KC_W);
    }

    if (socd_a_active) {
        register_code(KC_A);
    } else {
        unregister_code(KC_A);
    }

    if (socd_s_active) {
        register_code(KC_S);
    } else {
        unregister_code(KC_S);
    }

    if (socd_d_active) {
        register_code(KC_D);
    } else {
        unregister_code(KC_D);
    }
}

static void enter_game_mode(void) {
    saved_layers = layer_state;
    layer_state_t new_state = (1UL << WIN_BASE) | (1UL << WIN_GAME);
    layer_state_set(new_state);
    game_mode = true;
    apply_socd_to_os();
    uint8_t saved_mods = get_mods();
    clear_mods();
    tap_code(KC_F24);
    set_mods(saved_mods);
}

static void exit_game_mode(void) {
    socd_w_active = false;
    socd_a_active = false;
    socd_s_active = false;
    socd_d_active = false;
    layer_state_set(saved_layers);
    game_mode = false;
    uint8_t saved_mods = get_mods();
    clear_mods();
    tap_code(KC_F23);
    set_mods(saved_mods);
}

static bool handle_game_mode_exit(uint16_t keycode, bool pressed, bool is_home_row_exit) {
    if (!pressed) return false;

    if (keycode == KC_LGUI || keycode == KC_RGUI || keycode == KC_LWIN || keycode == KC_RWIN) {
        exit_game_mode();
        return true;
    }
    if (is_home_row_exit) {
        exit_game_mode();
        return true;
    }
    return false;
}

static void handle_game_mode_entry(uint8_t wasd_key, bool pressed) {
    if (game_mode || !pressed) return;

    if (wasd_key) {
        wasd_keys_pressed |= (1 << (wasd_key - 1));
        if (wasd_key != last_wasd_key) {
            wasd_alternating_count++;
            last_wasd_key = wasd_key;
        }
        if (wasd_keys_pressed == 0x0F || wasd_alternating_count >= 5) {
            reset_wasd_tracking();
            enter_game_mode();
        }
    } else {
        reset_wasd_tracking();
    }
}

static void handle_home_row_mod_press(uint16_t keycode, uint8_t row, uint8_t col) {
    switch (keycode) {
        case HM_A:
            register_mod_at_position(row, col, MOD_BIT(KC_LGUI));
            break;
        case HM_S:
            register_mod_at_position(row, col, MOD_BIT(KC_LALT));
            break;
        case HM_D:
            register_mod_at_position(row, col, MOD_BIT(KC_LCTL));
            break;
        case HM_F:
            register_mod_at_position(row, col, MOD_BIT(KC_LSFT));
            break;
        case HM_J:
            register_mod_at_position(row, col, MOD_BIT(KC_RSFT));
            break;
        case HM_K:
            register_mod_at_position(row, col, MOD_BIT(KC_RCTL));
            break;
        case HM_L:
            register_mod_at_position(row, col, MOD_BIT(KC_RALT));
            break;
        case HM_SCL:
            register_mod_at_position(row, col, MOD_BIT(KC_RGUI));
            break;
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    uint8_t row = record->event.key.row;
    uint8_t col = record->event.key.col;
    uint8_t wasd_key = get_wasd_key(row, col);
    bool is_home_row_exit = (row == 3 && (col >= 7 && col <= 10));

    update_socd_state(wasd_key, record->event.pressed);

    if (game_mode) {
        apply_socd_to_os();
        if (handle_game_mode_exit(keycode, record->event.pressed, is_home_row_exit)) {
            return true;
        }
    }

    if (!record->event.pressed) {
        unregister_mod_at_position(row, col);
    }

    handle_game_mode_entry(wasd_key, record->event.pressed);

    if (record->event.pressed) {
        switch (keycode) {
            case PASS:
                SEND_STRING(SECRET_PASS);
                return false;
        }
    }


    if (keycode == SOCD_W || keycode == SOCD_A || keycode == SOCD_S || keycode == SOCD_D) {
        return false;
    }

    if (record->event.pressed) {
        handle_home_row_mod_press(keycode, row, col);
    }

    return true;
}
