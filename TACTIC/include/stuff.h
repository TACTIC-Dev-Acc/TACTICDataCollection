#pragma once

inline float QTimer()
{
	using func_t = decltype(&QTimer);
	static REL::Relocation<func_t> func{ RELOCATION_ID(47089, 48253) };
	return func();
}
