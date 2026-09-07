#pragma once

#include <string_view>
#include "gfx/font.h"

class IRenderer;

/// The two baked faces the whole UI draws with, loaded from the system Fonts directory.
///
///   Body      - anything the user reads or types as content: usernames, notes, field values,
///               dialog titles, button labels.
///   Secondary - the static chrome around it: row labels, hints, footers. Never a value the
///               user typed.
///
/// Secondary is independently sized and persisted rather than a fixed ratio of body, because a
/// genuinely smaller face reads better than shrinking the body face's quads.
///
/// Both are DPI-aware by construction, so this class is the only place a DPI scale is threaded
/// in and every consumer keeps reasoning in logical pixels.
class CFontManager {
  public:
	/// The startup bake, before settings have been loaded; ApplyBody immediately overrides it
	/// with the persisted values.
	bool Load(IRenderer *pRenderer, float dpiScale);

	/// Re-bakes both faces. Leaves both untouched if either fails - a bad font name typed into
	/// the settings field, or a transient failure after a DPI change, must never leave the UI
	/// without a working font.
	bool ApplyBody(IRenderer *pRenderer, std::string_view fontFileName, float bodyPixelSize, float secondaryPixelSize,
				   float dpiScale);

	const CFont &GetBody() const
	{
		return m_body;
	}

	const CFont &GetSecondary() const
	{
		return m_secondary;
	}

  private:
	CFont m_body;
	CFont m_secondary;
};
