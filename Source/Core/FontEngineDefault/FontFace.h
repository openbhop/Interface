#pragma once

#include "../../../Include/RmlUi/Core/StyleTypes.h"
#include "FontTypes.h"

namespace Rml {

class FontFaceHandleDefault;

class FontFace {
public:
	FontFace(FontFaceHandleFreetype face, Style::FontStyle style, Style::FontWeight weight);
	~FontFace();

	Style::FontStyle GetStyle() const;
	Style::FontWeight GetWeight() const;

	/// Returns a handle for positioning and rendering this face at the given size.
	/// @param[in] size The size of the desired handle, in points.
	/// @param[in] load_default_glyphs True to load the default set of glyph (ASCII range).
	/// @param[in] synthetic_weight_delta If non-zero and positive, the face will be synthetically emboldened to better
	/// match the requested weight when that weight is not available.
	/// @return The font handle.
	FontFaceHandleDefault* GetHandle(int size, bool load_default_glyphs, int synthetic_weight_delta);

	/// Releases resources owned by sized font faces, including their textures and rendered glyphs.
	void ReleaseFontResources();

private:
	Style::FontStyle style;
	Style::FontWeight weight;

	// Key is (font size, synthetic weight delta). We include the synthetic adjustment in the key to allow
	// multiple handles at the same size with different synthetic weights.
	using HandleKey = uint64_t;
	using HandleMap = UnorderedMap<HandleKey, UniquePtr<FontFaceHandleDefault>>;
	HandleMap handles;

	FontFaceHandleFreetype face;
};

} // namespace Rml
