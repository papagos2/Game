// Writes every procedural icon and decal into one contact-sheet image (PPM on a checker
// background) so the icon set can be reviewed. Usage: bhicons <out.ppm> [size]
#include "BhPainter.h"

#include <cstdio>
#include <cstdlib>

using namespace bh;

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: bhicons <out.ppm> [size]\n");
		return 1;
	}
	const int Size = Argc > 2 ? std::atoi(Argv[2]) : 96;
	const int Cols = 12;
	const int NumDecals = static_cast<int>(DecalTex::Count);
	const int Total = NumIcons - 1 + NumDecals;
	const int Rows = (Total + Cols - 1) / Cols;
	const int W = Cols * (Size + 8);
	const int H = Rows * (Size + 8);
	std::vector<unsigned char> Sheet(static_cast<size_t>(W * H * 3));
	for (int Y = 0; Y < H; ++Y)
	{
		for (int X = 0; X < W; ++X)
		{
			const bool Dark = ((X / 12) + (Y / 12)) % 2 == 0;
			unsigned char* P = &Sheet[static_cast<size_t>((Y * W + X) * 3)];
			P[0] = Dark ? 52 : 66;
			P[1] = Dark ? 44 : 56;
			P[2] = Dark ? 62 : 76;
		}
	}
	for (int K = 0; K < Total; ++K)
	{
		ImageRGBA Img;
		if (K < NumIcons - 1)
		{
			PaintIcon(static_cast<Icon>(K + 1), Size, Img);
		}
		else
		{
			PaintDecal(static_cast<DecalTex>(K - (NumIcons - 1)), Size, Img);
		}
		const int Ox = (K % Cols) * (Size + 8) + 4;
		const int Oy = (K / Cols) * (Size + 8) + 4;
		for (int Y = 0; Y < Size; ++Y)
		{
			for (int X = 0; X < Size; ++X)
			{
				const unsigned char* S = &Img.Px[static_cast<size_t>((Y * Size + X) * 4)];
				unsigned char* D = &Sheet[static_cast<size_t>(((Oy + Y) * W + Ox + X) * 3)];
				const float A = S[3] / 255.f;
				for (int C = 0; C < 3; ++C)
				{
					D[C] = static_cast<unsigned char>(S[C] * A + D[C] * (1.f - A));
				}
			}
		}
	}
	FILE* F = std::fopen(Argv[1], "wb");
	if (F == nullptr)
	{
		return 1;
	}
	std::fprintf(F, "P6\n%d %d\n255\n", W, H);
	std::fwrite(Sheet.data(), 1, Sheet.size(), F);
	std::fclose(F);
	return 0;
}
