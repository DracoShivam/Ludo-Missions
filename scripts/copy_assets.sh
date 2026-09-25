#!/usr/bin/env bash
# Copies the minimal chaupar assets into Content/ (committed). Fails loudly if a source is missing.
set -euo pipefail
cd "$(dirname "$0")/.."
A=chaupar/StudioProject/AssetSrc/resizeables
R=chaupar/Resources
mkdir -p Content/images/{dice,tokens,board,ui} Content/fonts Content/sounds
for i in 1 2 3 4 5 6; do
	cp "$A/game/dice$i.png" "$A/game/diceroll$i.png" Content/images/dice/
done
cp "$A/game/token_ludo_basic_base.png" "$A/game/token_ludo_basic_color.png" "$A/game/token_basic_shine.png" Content/images/tokens/
cp "$A/game/star_outlined.png" "$A/game/box.png" "$A/game/triangle.png" Content/images/board/
cp "$A/common/coin.png" "$A/common/big_coin.png" "$A/common/circle.png" "$A/common/sparkle_star.png" Content/images/ui/
cp "$A/scale9/flat_round_white.png" Content/images/ui/
cp "$R/fonts/mikado_bold.ttf" "$R/fonts/mikado_black.ttf" "$R/fonts/luckiest_guy.ttf" Content/fonts/
for s in dice_roll token_move kill_alter my_turn celebrate_win coin_collect; do
	cp "$R/sound/$s.mp3" Content/sounds/
done
echo "Assets copied."
