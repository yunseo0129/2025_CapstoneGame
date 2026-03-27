#include "CBinary_Converter.h"

int main()
{
	CBinary_Converter converter;
	converter.Convert(MODEL_TYPE::TYPE_NONANIM, L"Input/NonAnim/");
	converter.Convert(MODEL_TYPE::TYPE_ANIM, L"Resources/Anim/");

	return 0;
}