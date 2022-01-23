#include "pch.h"

#include "ChannelsTable.h"

void print(ChannelsTable df) {
	for (auto [index, record] : df) {
		auto [subimage, idx] = index;
		auto [layer, view, channel] = record;
		std::cout << "{{" << subimage << "," << idx << "},{\"" << layer << "\",\"" << view << "\",\"" << channel << "\"}},\n";
	}
}

TEST(TestCaseName, TestName)
{

	ChannelsTable df = get_channelstable("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0001.exr");
	//print(df);
	ChannelsTable expected{
		{{0,0},{"[color]","right","R"}},
		{{0,1},{"[color]","right","G"}},
		{{0,2},{"[color]","right","B"}},
		{{0,3},{"[color]","right","A"}},
		{{0,4},{"[depth]","right","Z"}},
		{{0,5},{"disparityL","","x"}},
		{{0,6},{"disparityL","","y"}},
		{{0,7},{"disparityR","","x"}},
		{{0,8},{"disparityR","","y"}},
		{{0,9},{"forward","left","u"}},
		{{0,10},{"forward","left","v"}},
		{{0,11},{"forward","right","u"}},
		{{0,12},{"forward","right","v"}},
		{{0,13},{"[color]","left","R"}},
		{{0,14},{"[color]","left","G"}},
		{{0,15},{"[color]","left","B"}},
		{{0,16},{"[color]","left","A"}},
		{{0,17},{"[depth]","left","Z"}},
		{{0,18},{"whitebarmask","left","mask"}},
		{{0,19},{"whitebarmask","right","mask"}}
	};

	EXPECT_TRUE(df == expected);
}