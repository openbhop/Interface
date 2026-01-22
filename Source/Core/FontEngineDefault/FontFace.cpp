#include "FontFace.h"
#include "../../../Include/RmlUi/Core/Log.h"
#include "../../../Include/RmlUi/Core/Math.h"
#include "FontFaceHandleDefault.h"
#include "FreeTypeInterface.h"

namespace Rml {

FontFace::FontFace(FontFaceHandleFreetype _face, Style::FontStyle _style, Style::FontWeight _weight)
{
	style = _style;
	weight = _weight;
	face = _face;
}

FontFace::~FontFace()
{
	if (face)
		FreeType::ReleaseFace(face);
}

Style::FontStyle FontFace::GetStyle() const
{
	return style;
}

Style::FontWeight FontFace::GetWeight() const
{
	return weight;
}

namespace {
	inline uint64_t MakeHandleKey(int size, int synthetic_weight_delta)
	{
		// Pack the key into 64 bits. This ensures stable hashing while keeping the key compact.
		const uint32_t u_size = (uint32_t)Math::Max(size, 0);
		const uint32_t u_delta = (uint32_t)Math::Max(synthetic_weight_delta, 0);
		return (uint64_t(u_size) << 32) | uint64_t(u_delta);
	}
} // namespace

FontFaceHandleDefault* FontFace::GetHandle(int size, bool load_default_glyphs, int synthetic_weight_delta)
{
	synthetic_weight_delta = Math::Max(synthetic_weight_delta, 0);
	const HandleKey key = (HandleKey)MakeHandleKey(size, synthetic_weight_delta);

	auto it = handles.find(key);
	if (it != handles.end())
		return it->second.get();

	// See if this face has been released.
	if (!face)
	{
		Log::Message(Log::LT_WARNING, "Font face has been released, unable to generate new handle.");
		return nullptr;
	}

	// Construct and initialise the new handle.
	auto handle = MakeUnique<FontFaceHandleDefault>();
	if (!handle->Initialize(face, size, load_default_glyphs, synthetic_weight_delta))
	{
		handles[key] = nullptr;
		return nullptr;
	}

	FontFaceHandleDefault* result = handle.get();

	// Save the new handle to the font face
	handles[key] = std::move(handle);

	return result;
}

void FontFace::ReleaseFontResources()
{
	HandleMap().swap(handles);
}

} // namespace Rml
