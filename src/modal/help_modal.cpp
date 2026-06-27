#include "modal/help_modal.hpp"

#include "modal/modal_base.hpp"
#include "modal/modals.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <imgui.h>

#include "bridge/focus_registry.hpp"

// Zone 11 — Equation Reference (Help) modal body. Read-only, scrollable, five
// stacked sections + the "Open Tutorial" button. The Aggressor formula text +
// grading margins are quoted from ARCHITECTURE L521 / L563; the Caller formulas
// (Pot Odds, Rule of 2 & 4 equity, net-call EV) mirror the engine's Module-1 math
// so Help matches the grader (engine/evaluator.hpp: pot_odds_fraction, net_call_ev;
// equity_from_outs in the generator). Content text is fixed (not theme-controlled);
// all colors come from tokens.

namespace poker_trainer::modal {

namespace {

void section_header(const char* title) {
    ImGui::Dummy(ImVec2{0.0f, 4.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::AccentPrimary));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

// Stacked, full modal width: the complete formula on its own line (monospace intent;
// the visual pass supplies the mono font), the gloss directly beneath in
// text_secondary, then spacing. Full width guarantees the long Semi-Bluff line is
// never column-clipped (it wraps rather than truncates if it ever exceeds the width).
void formula_entry(const char* formula, const char* gloss) {
    ImGui::TextWrapped("%s", formula);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    ImGui::TextWrapped("%s", gloss);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2{0.0f, 6.0f});
}

void definition(const char* term, const char* body) {
    ImGui::TextUnformatted(term);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    ImGui::TextWrapped("%s", body);
    ImGui::PopStyleColor();
}

void draw_help_body() {
    section_header("Formulas");
    section_header("Aggressor");
    formula_entry("1. Pure Bluff EV = P(fold) x pot - P(call) x bet",
                  "Bet to make a better hand fold; you win the pot when they fold and lose the bet "
                  "when they call.");
    formula_entry("2. Value Bet EV = P(call) x bet",
                  "Bet hoping a worse hand calls; your profit is the called bet.");
    formula_entry("3. Semi-Bluff EV = P(fold) x pot + P(call) x [equity x (pot + 2 x bet) - bet]",
                  "A draw with fold equity: win now when they fold, otherwise realize your equity "
                  "in the larger pot.");
    section_header("Caller");
    formula_entry("1. Pot Odds = bet / (pot + bet)",
                  "The price you're getting on a call. Lower means cheaper to call.");
    formula_entry("2. Equity (estimate) = outs x 4 on the flop, outs x 2 on the turn",
                  "The rule of 2 and 4: your approximate chance to improve, from your number of "
                  "outs.");
    formula_entry("3. Outs",
                  "The number of unseen cards that complete your hand.");
    formula_entry("4. Caller EV = equity x (pot + bet) - (1 - equity) x bet",
                  "The net value of calling: you win the pot plus the bet with your equity, and "
                  "lose the bet otherwise.");
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    ImGui::TextWrapped("Conventions: pot is the pot before your bet; bet is your wager (the Caller's "
                       "bet is the call faced); P(fold) and P(call) are the opponent's probabilities; "
                       "equity is your share when called; outs are the unseen cards that improve you.");
    ImGui::PopStyleColor();

    section_header("Math Inputs");
    definition("Pot Odds", "The price you are being laid to call: the call relative to the pot you "
                           "stand to win. Tells you the equity you need to break even.");
    definition("Outs", "The number of unseen cards that improve you to the best hand. The raw count "
                       "behind your equity.");
    definition("Equity", "Your percentage chance to win the hand if it goes to showdown.");
    definition("EV", "Expected value: the average dollar result of a decision over the long run.");
    definition("Fold Probability", "How often you expect the opponent to fold to your bet.");
    definition("Bet Size", "The sizing choice (relative to the pot) that maximizes your EV.");

    section_header("Scenario Types");
    definition("Caller", "You face a bet and decide whether the price is right to continue. Visual "
                        "cue: an opponent's chips pushed forward. Answer: Pot Odds, Outs, Equity.");
    definition("Aggressor", "You are betting; pick the line and size. Visual cue: the action is on "
                           "you with no bet to face. Answer: Fold Probability, EV, and Bet Size.");
    definition("  - Pure Bluff", "No showdown value; you need folds. Answer P(fold) and EV.");
    definition("  - Value Bet", "You want a worse hand to call. Answer P(call) and EV.");
    definition("  - Semi-Bluff", "A draw with fold equity. Answer P(fold), equity-if-called, EV.");

    section_header("Grading Rules");
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    ImGui::TextWrapped("Dollar EV: within +/-5%% (relative), with a $0.50 absolute floor.\n"
                       "Probabilities (Pot Odds, Equity, P(fold)): within +/-5 percentage points.\n"
                       "Outs: exact integer match.\n"
                       "Bet Size: exact button selection match.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2{0.0f, 8.0f});
}

}  // namespace

void render_help_modal() {
    const std::uint32_t ring =
        ImGui::ColorConvertFloat4ToU32(theme::get_color(theme::ColorToken::BorderFocus));

    const bool visible =
        modal_begin_centered("##help_modal", kClusterModalWidthFrac, kClusterModalHeightFrac);
    bool dismiss = false;
    bool start_tutorial = false;
    if (visible) {
        const bool x_clicked = modal_draw_x_close(kHelpClose);
        modal_draw_pill_header(assets::AssetId::IconHelp, "Help");
        modal_draw_lock_banner();

        // Scrollable read-only body.
        if (ImGui::BeginChild("##help_body", ImVec2{0.0f, -ImGui::GetFrameHeightWithSpacing() * 1.4f},
                              false)) {
            draw_help_body();
        }
        ImGui::EndChild();

        // Full-width "Open Tutorial" button (disabled during an active tutorial).
        modal_begin_locked_controls();
        const bool tut_clicked = ImGui::Button("Open Tutorial", ImVec2{-1.0f, 0.0f});
        modal_end_locked_controls();
        bridge::draw_focus_ring(kHelpTutorial, ring);

        if (x_clicked || modal_click_outside_dismissed()) {
            dismiss = true;
        } else if (tut_clicked && !modal_is_locked()) {
            start_tutorial = true;
        }
    }
    modal_end();

    if (dismiss) {
        close_modal();
    } else if (start_tutorial) {
        close_modal();
        // SEAM(Z14): tutorial_start() — initiate the tutorial overlay flow. Inert
        // until Zone 14 lands.
    }
}

}  // namespace poker_trainer::modal
