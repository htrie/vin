
class Html {
	enum class Tag {
		none,
		tag,
		skip,
		address_header,
		address_header_href,
		address_header_href_value,
		address_body,
		address_footer,
		paragraph_header,
		paragraph_body,
		paragraph_body_unicode,
		paragraph_footer,
		paragraph_nested_header,
		paragraph_nested_body,
		paragraph_nested_footer,
	};

	Tag tag = Tag::none;

	HugeString address_body;
	HugeString address_value;

	HugeString paragraph_body;
	size_t paragraph_line_size = 0;

	HugeString result;

	void print_address() {
		if (!address_body.empty() && address_value.starts_with("http"))
			result += address_body + " " + address_value + "\n";
	}

	void print_paragraph() {
		if (!paragraph_body.empty())
			result += paragraph_body + "\n\n";
	}

	void append_paragraph(const char c) {
		if (paragraph_line_size > 80 && c == ' ') {
			paragraph_body += '\n';
			paragraph_line_size = 0;
		}
		else {
			paragraph_body += c;
			paragraph_line_size++;
		}
	}

	void process_address_footer(const HugeString& text, size_t pos) {
		if (text[pos] == '>') { print_address(); tag = Tag::none; }
	}
	void process_address_body(const HugeString& text, size_t pos) {
		if (text[pos] == '<') { tag = Tag::address_footer; }
		else if (text[pos] == '\n') { /* Skip */ }
		else if (text[pos] == 13/*CR*/) { /* Skip */ }
		else if (text[pos] == '\t') { /* Skip */ }
		else { address_body += text[pos]; }
	}
	void process_address_header_href_value(const HugeString& text, size_t pos) {
		if (text[pos] == '"') { tag = Tag::address_header; }
		else { address_value += text[pos]; }
	}
	void process_address_header_href(const HugeString& text, size_t pos) {
		if (text[pos] == '=') {}
		else if (text[pos] == '"') { address_value.clear(); tag = Tag::address_header_href_value; }
	}
	void process_address_header(const HugeString& text, size_t pos) {
		if (text[pos] == '>') { address_body.clear(); tag = Tag::address_body; }
		else if (text.substr(pos, 4) == SmallString("href")) { tag = Tag::address_header_href; }
	}

	void process_paragraph_nested_footer(const HugeString& text, size_t pos) {
		if (text[pos] == '>') { tag = Tag::paragraph_body; }
	}
	void process_paragraph_nested_body(const HugeString& text, size_t pos) {
		if (text[pos] == '<') { tag = Tag::paragraph_nested_footer; }
		else if (text[pos] == '\n') { append_paragraph(' '); }
		else if (text[pos] == 13/*CR*/) { append_paragraph(' '); }
		else if (text[pos] == '\t') { append_paragraph(' '); }
		else { append_paragraph(text[pos]); }
	}
	void process_paragraph_nested_header(const HugeString& text, size_t pos) {
		if (text[pos] == '>') { tag = Tag::paragraph_nested_body; }
	}

	void process_paragraph_footer(const HugeString& text, size_t pos) {
		if (text[pos] == '>') { tag = Tag::none; }
	}
	void process_paragraph_body_unicode(const HugeString& text, size_t pos) {
		if (text[pos] == -128) { /* Skip */ }
		else if (text[pos] == -90) { append_paragraph('.'); append_paragraph('.'); append_paragraph('.'); tag = Tag::paragraph_body; }
		else if (text[pos] == -99) { append_paragraph('"'); tag = Tag::paragraph_body; }
		else if (text[pos] == -100) { append_paragraph('"'); tag = Tag::paragraph_body; }
		else if (text[pos] == -103) { append_paragraph('\''); tag = Tag::paragraph_body; }
		else if (text[pos] == -108) { append_paragraph('-'); tag = Tag::paragraph_body; }
		else { append_paragraph(text[pos]); tag = Tag::paragraph_body; }
	}
	void process_paragraph_body(const HugeString& text, size_t pos) {
		if (text.substr(pos, 4) == SmallString("<em>")) { tag = Tag::paragraph_nested_header; }
		else if (text.substr(pos, 6) == SmallString("<code>")) { tag = Tag::paragraph_nested_header; }
		else if (text.substr(pos, 3) == SmallString("<b>")) { tag = Tag::paragraph_nested_header; }
		else if (text.substr(pos, 3) == SmallString("<i>")) { tag = Tag::paragraph_nested_header; }
		else if (text.substr(pos, 4) == SmallString("<a >")) { tag = Tag::paragraph_nested_header; }
		else if (text[pos] == '<') { print_paragraph(); tag = Tag::paragraph_footer; }
		else if (text[pos] == -30) { tag = Tag::paragraph_body_unicode; }
		else if (text[pos] == '\n') { append_paragraph(' '); }
		else if (text[pos] == 13/*CR*/) { append_paragraph(' '); }
		else if (text[pos] == '\t') { append_paragraph(' '); }
		else { append_paragraph(text[pos]); }
	}
	void process_paragraph_header(const HugeString& text, size_t pos) {
		if (text[pos] == '>') { paragraph_body.clear(); paragraph_line_size = 0; tag = Tag::paragraph_body; }
	}

	void process_skip(const HugeString& text, size_t pos) {
		if (text[pos] == '>') { tag = Tag::none; }
	}

	void process_tag(const HugeString& text, size_t pos) {
		if (text[pos] == 'a') { tag = Tag::address_header; }
		else if (text[pos] == 'p') { tag = Tag::paragraph_header; }
		else { tag = Tag::skip; }
	}

	void process_none(const HugeString& text, size_t pos) {
		if (text[pos] == '<') { tag = Tag::tag; }
	}

	void process_chars(const HugeString& text, size_t pos) {
		switch (tag) {
			case Tag::none: process_none(text, pos); break;
			case Tag::tag: process_tag(text, pos); break;
			case Tag::skip: process_skip(text, pos); break;
			case Tag::address_header: process_address_header(text, pos); break;
			case Tag::address_header_href: process_address_header_href(text, pos); break;
			case Tag::address_header_href_value: process_address_header_href_value(text, pos); break;
			case Tag::address_body: process_address_body(text, pos); break;
			case Tag::address_footer: process_address_footer(text, pos); break;
			case Tag::paragraph_header: process_paragraph_header(text, pos); break;
			case Tag::paragraph_body: process_paragraph_body(text, pos); break;
			case Tag::paragraph_body_unicode: process_paragraph_body_unicode(text, pos); break;
			case Tag::paragraph_footer: process_paragraph_footer(text, pos); break;
			case Tag::paragraph_nested_header: process_paragraph_nested_header(text, pos); break;
			case Tag::paragraph_nested_body: process_paragraph_nested_body(text, pos); break;
			case Tag::paragraph_nested_footer: process_paragraph_nested_footer(text, pos); break;
		}
	}

public:
	HugeString process(const HugeString& text) {
		for (size_t i = 0; i < text.size(); ++i) {
			process_chars(text, i);
		}
		return result;
	}
};

