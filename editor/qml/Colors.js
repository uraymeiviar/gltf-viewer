const dark0 = "#212121";
const dark05 = "#272727";
const dark1 = "#2b2b2b";
const dark2 = "#373737";
const dark205 = "#3b3b3b";
const dark3 = "#424242";
const dark305 = "#494949";
const dark4 = "#4d4d4d";
const dark5 = "#666666";

const lightgreen = "#9CCD63";
const green = "#87C540";
const green3 = "#73A43A";
const darkgreen = "#72A339";
const darkgreen2 = "#44592B";
const darkgreen3 = "#3F4834";

const light0 = "#ffffff";
const light1 = "#f4f4f4";
const light3 = "#d3d3d3";
const light4 = "#909090";
const light5 = "#BCBCBC";

const red = "#FF3434";
const red1 = "#F85E5E";
const red2 = "#642626";
const red3 = "#F05C5C";

const redlight1 = "#FF9999";

function getInspectorRowColor(depth, hovered) {
  if (depth === -1) {
    return hovered ? dark3 : dark2;
  } else if (depth === 0) {
    return hovered ? dark4 : dark3;
  } else if (depth === 1) {
    return hovered ? dark4 : dark305;
  } else {
    return hovered ? dark3 : dark205;
  }
}
