
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
			result += paragraph_body + "\n";
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

	void process_address_footer(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { print_address(); tag = Tag::none; }
	}
	void process_address_body(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '<') { tag = Tag::address_footer; }
		else if (c0 == '\n') { /* Skip */ }
		else if (c0 == 13/*CR*/) { /* Skip */ }
		else if (c0 == '\t') { /* Skip */ }
		else { address_body += c0; }
	}
	void process_address_header_href_value(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '"') { tag = Tag::address_header; }
		else { address_value += c0; }
	}
	void process_address_header_href(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '=') {}
		else if (c0 == '"') { address_value.clear(); tag = Tag::address_header_href_value; }
	}
	void process_address_header(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { address_body.clear(); tag = Tag::address_body; }
		else if (c0 == 'h' && c1 == 'r' && c2 == 'e' && c3 == 'f') { tag = Tag::address_header_href; }
	}

	void process_paragraph_nested_footer(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { tag = Tag::paragraph_body; }
	}
	void process_paragraph_nested_body(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '<') { tag = Tag::paragraph_nested_footer; }
		else if (c0 == '\n') { /* Skip */ }
		else if (c0 == 13/*CR*/) { /* Skip */ }
		else if (c0 == '\t') { /* Skip */ }
		else { append_paragraph(c0); }
	}
	void process_paragraph_nested_header(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { tag = Tag::paragraph_nested_body; }
	}

	void process_paragraph_footer(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { tag = Tag::none; }
	}
	void process_paragraph_body(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '<' && c1 == 'e' && c2 == 'm' && c3 == '>') { tag = Tag::paragraph_nested_header; }
		else if (c0 == '<' && c1 == 'c' && c2 == 'o' && c3 == 'd') { tag = Tag::paragraph_nested_header; }
		else if (c0 == '<' && c1 == 'b' && c2 == '>') { tag = Tag::paragraph_nested_header; }
		else if (c0 == '<' && c1 == 'i' && c2 == '>') { tag = Tag::paragraph_nested_header; }
		else if (c0 == '<' && c1 == 'a' && c2 == ' ') { tag = Tag::paragraph_nested_header; }
		else if (c0 == '<') { print_paragraph(); tag = Tag::paragraph_footer; }
		else if (c0 == '\n') { /* Skip */ }
		else if (c0 == 13/*CR*/) { /* Skip */ }
		else if (c0 == '\t') { /* Skip */ }
		else { append_paragraph(c0); }
	}
	void process_paragraph_header(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { paragraph_body.clear(); paragraph_line_size = 0; tag = Tag::paragraph_body; }
	}

	void process_skip(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '>') { tag = Tag::none; }
	}

	void process_tag(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == 'a') { tag = Tag::address_header; }
		else if (c0 == 'p') { tag = Tag::paragraph_header; }
		else { tag = Tag::skip; }
	}

	void process_none(const char c0, const char c1, const char c2, const char c3) {
		if (c0 == '<') { tag = Tag::tag; }
	}

	void process_chars(const char c0, const char c1, const char c2, const char c3) {
		switch (tag) {
			case Tag::none: process_none(c0, c1, c2, c3); break;
			case Tag::tag: process_tag(c0, c1, c2, c3); break;
			case Tag::skip: process_skip(c0, c1, c2, c3); break;
			case Tag::address_header: process_address_header(c0, c1, c2, c3); break;
			case Tag::address_header_href: process_address_header_href(c0, c1, c2, c3); break;
			case Tag::address_header_href_value: process_address_header_href_value(c0, c1, c2, c3); break;
			case Tag::address_body: process_address_body(c0, c1, c2, c3); break;
			case Tag::address_footer: process_address_footer(c0, c1, c2, c3); break;
			case Tag::paragraph_header: process_paragraph_header(c0, c1, c2, c3); break;
			case Tag::paragraph_body: process_paragraph_body(c0, c1, c2, c3); break;
			case Tag::paragraph_footer: process_paragraph_footer(c0, c1, c2, c3); break;
			case Tag::paragraph_nested_header: process_paragraph_nested_header(c0, c1, c2, c3); break;
			case Tag::paragraph_nested_body: process_paragraph_nested_body(c0, c1, c2, c3); break;
			case Tag::paragraph_nested_footer: process_paragraph_nested_footer(c0, c1, c2, c3); break;
		}
	}

public:
	HugeString process(const HugeString& text) {
		for (size_t i = 0; i < text.size(); ++i) {
			const auto c0 = i < text.size() - 0 ? text[i + 0] : '0';
			const auto c1 = i < text.size() - 1 ? text[i + 1] : '1';
			const auto c2 = i < text.size() - 2 ? text[i + 2] : '2';
			const auto c3 = i < text.size() - 3 ? text[i + 3] : '3';
			process_chars(c0, c1, c2, c3);
		}
		return result;
	}
};

