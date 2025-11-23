#include QMK_KEYBOARD_H

enum layers {
    WIN_BASE,
    WIN_HRM,
    WIN_NAV,
    WIN_GAME,
    WIN_FN,
};

enum custom_keycodes {
    SOCD_W = SAFE_RANGE,
    SOCD_A,
    SOCD_S,
    SOCD_D,
    SOCD_ON,
    SOCD_OFF,
    GAME_TOG,
};

static bool w_down = false;
static bool a_down = false;
static bool s_down = false;
static bool d_down = false;

static layer_state_t saved_layers = 0;
static bool          game_mode    = false;
static uint8_t       wasd_keys_pressed = 0;  // Bitmask: bit 0=W, 1=A, 2=S, 3=D
static uint8_t       wasd_alternating_count = 0;  // Count of alternating WASD presses
static uint8_t       last_wasd_key = 0;  // Track last WASD key pressed (1=W, 2=A, 3=S, 4=D)

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
        KC_LCTL,  KC_LWIN,  KC_LALT,                                KC_SPC,
                      KC_RALT,  MO(WIN_FN), KC_NO,    KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT),

    [WIN_HRM] = LAYOUT_tkl_ansi(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  HM_A,     HM_S,     HM_D,     HM_F,     _______,  _______,  HM_J,     HM_K,     HM_L,     HM_SCL,   _______,              _______,
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  _______,  MO(WIN_NAV),                            _______, 
                      _______,  _______,  _______,  _______,  _______,  _______,  _______),

    [WIN_NAV] = LAYOUT_tkl_ansi(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  KC_LGUI,  KC_LALT,  KC_LCTL,  KC_LSFT,  _______,  KC_LEFT,  KC_DOWN,  KC_UP,    KC_RGHT,  _______,  _______,              _______,
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  _______,  _______,                                _______,           _______,  _______,    _______,  _______,  _______,  _______,  _______),

    [WIN_GAME] = LAYOUT_tkl_ansi(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  SOCD_W,   _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  SOCD_A,   SOCD_S,   SOCD_D,   _______,  _______,  _______,  _______,  _______,  _______,  _______, _______,              _______,
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  _______,  _______,                                _______,            _______,  _______,  GAME_TOG,  _______,  _______,  _______,  _______),

    [WIN_FN] = LAYOUT_tkl_ansi(
        _______,            KC_BRID,  KC_BRIU,  KC_TASK,  KC_FLXP,  BL_DOWN,  BL_UP,    KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,    KC_VOLD,  KC_VOLU,  KC_PSCR,  _______,  BL_STEP,
        _______,  SOCD_ON,  SOCD_OFF, _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        BL_TOGG,  BL_STEP,  BL_UP,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,  _______,
        _______,  _______,  BL_DOWN,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,
        _______,            _______,  _______,  _______,  _______,  _______,  NK_TOGG,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  GU_TOGG,  _______,                                _______,                                _______,  _______,    _______,  _______,  _______,  _______,  _______)
};
// clang-format on


void keyboard_post_init_user(void) {
    layer_on(WIN_HRM);
}

// Helper function to enter game mode
static void enter_game_mode(void) {
    tap_code(KC_F24);
    saved_layers = layer_state;
    layer_state_t new_state = (1UL << WIN_BASE) | (1UL << WIN_GAME);
    layer_state_set(new_state);
    game_mode = true;
}

static void exit_game_mode(void) {
    tap_code(KC_F23);
    layer_state_set(saved_layers);
    game_mode = false;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Get the physical row and column of the pressed key
    uint8_t row = record->event.key.row;
    uint8_t col = record->event.key.col;

    // Physical key positions
    bool is_w = (row == 2 && col == 2);
    bool is_a = (row == 3 && col == 1);
    bool is_s = (row == 3 && col == 2);
    bool is_d = (row == 3 && col == 3);
    bool is_j = (row == 3 && col == 7);      // J key
    bool is_k = (row == 3 && col == 8);      // K key
    bool is_l = (row == 3 && col == 9);      // L key
    bool is_scln = (row == 3 && col == 10);  // ; key

    // Exit game mode on Windows/GUI key press
    if (game_mode && (keycode == KC_LGUI || keycode == KC_RGUI || keycode == KC_LWIN || keycode == KC_RWIN)) {
        if (record->event.pressed) {
            exit_game_mode();
        }
        return true;  // Process the key normally (will activate GUI)
    }

    // Exit game mode on right-hand homerow physical keys (J, K, L, ;) - these function as homerow mods
    if (game_mode && (is_j || is_k || is_l || is_scln)) {
        if (record->event.pressed) {
            exit_game_mode();
        }
        // Let QMK handle the homerow mod tap/hold logic in the HRM layer
        return true;
    }

    // WASD detection for game mode entry (using physical key positions)
    if (!game_mode && record->event.pressed) {
        bool is_wasd_key = is_w || is_a || is_s || is_d;

        if (is_wasd_key) {
            uint8_t current_key = is_w ? 1 : (is_a ? 2 : (is_s ? 3 : 4));

            // Method 1: Track all 4 unique keys pressed in any order
            if (is_w) wasd_keys_pressed |= (1 << 0);
            if (is_a) wasd_keys_pressed |= (1 << 1);
            if (is_s) wasd_keys_pressed |= (1 << 2);
            if (is_d) wasd_keys_pressed |= (1 << 3);

            // Check if all 4 keys have been pressed
            if (wasd_keys_pressed == 0x0F) {  // 0b1111 = all 4 bits set
                wasd_keys_pressed = 0;
                wasd_alternating_count = 0;
                last_wasd_key = 0;
                enter_game_mode();
                return true;
            }

            // Method 2: Track alternating presses (non-repeating)
            if (current_key != last_wasd_key) {
                wasd_alternating_count++;
                last_wasd_key = current_key;

                if (wasd_alternating_count >= 8) {
                    wasd_keys_pressed = 0;
                    wasd_alternating_count = 0;
                    last_wasd_key = 0;
                    enter_game_mode();
                    return true;
                }
            }
            // If same key pressed twice in a row, don't increment alternating count
        } else {
            // Non-WASD key pressed, reset all tracking
            wasd_keys_pressed = 0;
            wasd_alternating_count = 0;
            last_wasd_key = 0;
        }
    }

    switch (keycode) {
        // SOCD implementation (WASD resolution)
        case SOCD_W:
            if (record->event.pressed) {
                if (s_down) {
                    unregister_code(KC_S);
                }
                register_code(KC_W);
                w_down = true;
            } else {
                unregister_code(KC_W);
                w_down = false;
                if (s_down) {
                    register_code(KC_S);
                }
            }
            return false;

        case SOCD_A:
            if (record->event.pressed) {
                if (d_down) {
                    unregister_code(KC_D);
                }
                register_code(KC_A);
                a_down = true;
            } else {
                unregister_code(KC_A);
                a_down = false;
                if (d_down) {
                    register_code(KC_D);
                }
            }
            return false;

        case SOCD_S:
            if (record->event.pressed) {
                if (w_down) {
                    unregister_code(KC_W);
                }
                register_code(KC_S);
                s_down = true;
            } else {
                unregister_code(KC_S);
                s_down = false;
                if (w_down) {
                    register_code(KC_W);
                }
            }
            return false;

        case SOCD_D:
            if (record->event.pressed) {
                if (a_down) {
                    unregister_code(KC_A);
                }
                register_code(KC_D);
                d_down = true;
            } else {
                unregister_code(KC_D);
                d_down = false;
                if (a_down) {
                    register_code(KC_A);
                }
            }
            return false;

        case SOCD_ON:
            if (record->event.pressed) {
                layer_on(WIN_GAME);
            }
            return false;

        case SOCD_OFF:
            if (record->event.pressed) {
                layer_off(WIN_GAME);
            }
            return false;

        case GAME_TOG:
            if (record->event.pressed) {
                if (game_mode) {
                    exit_game_mode();
                } else {
                    enter_game_mode();
                }
            }
            return false;
    }

    return true;
}
