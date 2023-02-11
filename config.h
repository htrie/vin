#pragma once

uint8_t channel(float c) { return (uint8_t)std::clamp(c, 0.0f, 255.0f); }

Color base_color(float f) { return Color::rgba(channel(244.0f * f), channel(240.0f * f), channel(222.0f * f), 255); }
Color text_color(float f) { return Color::rgba(channel(68.0f * f), channel(68.0f * f), channel(68.0f * f), 255); }

struct Colors {
	Color clear = base_color(1.0f);
	Color column_indicator = base_color(0.97f);
	Color overlay = base_color(0.95f);
	Color cursor_line = base_color(0.9f);
	Color cursor = base_color(0.5f);
	Color highlight = base_color(0.8f);
	Color whitespace = base_color(1.0f);
	Color text = text_color(1.0f);
	Color text_cursor = base_color(1.0f);
	Color keyword = text_color(0.5f);
	Color punctuation = text_color(1.0f);
	Color number = text_color(1.0f);
	Color line_number = text_color(2.4f);
	Color comment = text_color(2.0f);
	Color diff_note = text_color(1.0f);
	Color diff_add = text_color(1.0f);
	Color diff_remove = text_color(2.4f);
};

Colors& colors() {
	static Colors colors;
	return colors;
}


struct Spacing {
	float character = 9.0f;
	float line = 20.0f;
	float zoom = 1.0f;
	unsigned tab = 3;

	std::string increase_char_width() { character = std::clamp(character + 0.05f, 0.0f, 20.0f); return std::string("char_width = ") + std::to_string(character); }
	std::string decrease_char_width() { character = std::clamp(character - 0.05f, 0.0f, 20.0f); return std::string("char_width = ") + std::to_string(character); }

	std::string increase_char_height() { line = std::clamp(line + 0.05f, 0.0f, 40.0f); return std::string("char_height = ") + std::to_string(line); }
	std::string decrease_char_height() { line = std::clamp(line - 0.05f, 0.0f, 40.0f); return std::string("char_height = ") + std::to_string(line); }

	std::string increase_char_zoom() { zoom = std::clamp(zoom + 0.05f, 0.0f, 2.0f); return std::string("char_zoom = ") + std::to_string(zoom); }
	std::string decrease_char_zoom() { zoom = std::clamp(zoom - 0.05f, 0.0f, 2.0f); return std::string("char_zoom = ") + std::to_string(zoom); }

	std::string increase_tab_size() { tab = std::clamp(tab + 1, 0u, 8u); return std::string("tab_size = ") + std::to_string(tab); }
	std::string decrease_tab_size() { tab = std::clamp(tab > 0 ? tab - 1 : 0u, 0u, 8u); return std::string("tab_size = ") + std::to_string(tab); }
};

Spacing& spacing() {
	static Spacing spacing;
	return spacing;
}

