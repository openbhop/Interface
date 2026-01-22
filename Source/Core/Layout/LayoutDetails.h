/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019-2023 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#pragma once

#include "../../../Include/RmlUi/Core/StyleTypes.h"
#include "../../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Box;

enum class BuildBoxMode {
	Inline,
	UnalignedBlock,
	Block,
};

/**
	Utility functions for building boxes from computed properties.

	Note: RmlUi's layout engine has been simplified to use Yoga as the sole
	foundation for layout. These utilities are retained for a small number of
	places where box construction is useful outside of the main layout pass
	(e.g. manual positioning and widget formatting).
 */
class LayoutDetails {
public:
	// Builds the box for an element.
	//   - containing_block is the size of the containing block used for resolving percentages.
	//   - content size is set to negative values when width/height is 'auto'.
	static void BuildBox(Box& box, Vector2f containing_block, Element* element, BuildBoxMode box_mode = BuildBoxMode::Block);

	// Returns a short string used for debugging, of the form: <tag id="..." class="...">.
	static String GetDebugElementName(Element* element);

	static bool IsScrollContainer(Style::Overflow overflow_x, Style::Overflow overflow_y)
	{
		return (overflow_x == Style::Overflow::Auto || overflow_x == Style::Overflow::Scroll || overflow_y == Style::Overflow::Auto ||
			overflow_y == Style::Overflow::Scroll);
	}
};

} // namespace Rml
