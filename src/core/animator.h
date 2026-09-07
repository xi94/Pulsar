#pragma once

/// Frame-rate independent easing, shared by every animated value in the UI. Enabled and
/// speed are global because they are user settings that apply everywhere at once.
class CAnimator {
  public:
	/// Exponential approach: returns `target` outright when animations are switched off.
	static float EaseToward(float value, float target, float rate, float deltaSeconds);

	static void SetEnabled(bool enabled)
	{
		s_bEnabled = enabled;
	}

	static bool IsEnabled()
	{
		return s_bEnabled;
	}

	static void SetSpeed(float speed)
	{
		s_flSpeed = speed;
	}

	static float GetSpeed()
	{
		return s_flSpeed;
	}

  private:
	static bool s_bEnabled;
	static float s_flSpeed;
};
