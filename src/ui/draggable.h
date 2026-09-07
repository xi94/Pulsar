#pragma once

/// "Press somewhere, track the pointer while held, tell a click apart from a real drag" - the
/// shared component behind the carousel's card drag, the mode-switcher thumb, the colour
/// picker's square and the settings sliders.
///
/// It holds no opinion about what the drag controls: a caller begins tracking on pointer-down,
/// feeds every move through Update, and reads the deltas back to apply whatever conversion is
/// specific to that widget.
class CDraggable {
  public:
	void Begin(float x, float y);

	/// Call on every pointer move while pressed. Once the pointer has travelled far enough,
	/// HasMoved latches true for the rest of this press - which is what keeps a plain click from
	/// being misread as a zero-length drag.
	void Update(float x, float y);

	void End();

	bool IsPressed() const
	{
		return m_bPressed;
	}

	bool HasMoved() const
	{
		return m_bHasMoved;
	}

	float StartX() const
	{
		return m_flStartX;
	}

	float StartY() const
	{
		return m_flStartY;
	}

	float DeltaX() const
	{
		return m_flCurrentX - m_flStartX;
	}

	float DeltaY() const
	{
		return m_flCurrentY - m_flStartY;
	}

  private:
	bool m_bPressed = false;
	bool m_bHasMoved = false;
	float m_flStartX = 0.0f;
	float m_flStartY = 0.0f;
	float m_flCurrentX = 0.0f;
	float m_flCurrentY = 0.0f;
};
