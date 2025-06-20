import React from 'react';
import {
  Modal,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from '@hippy/react';
import Long from 'long';

const SKIN_COLOR = {
  mainLight: '#4c9afa',
  otherLight: '#4c9afa',
  textWhite: 'white',
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    flexDirection: 'column',
    justifyContent: 'flex-start',
    alignItems: 'center',
  },
  buttonView: {
    borderColor: SKIN_COLOR.mainLight,
    borderWidth: 2,
    borderStyle: 'solid',
    borderRadius: 8,
    justifyContent: 'center',
    alignItems: 'center',
    width: 250,
    height: 50,
    marginTop: 30,
  },
  buttonText: {
    fontSize: 20,
    color: SKIN_COLOR.mainLight,
    textAlign: 'center',
    textAlignVertical: 'center',
  },
  selectionText: {
    fontSize: 20,
    textAlign: 'center',
    textAlignVertical: 'center',
    marginLeft: 10,
    marginRight: 10,
    padding: 5,
    borderRadius: 5,
    borderWidth: 2,
  },
});

const MAP_COLUMN_SIZE = 64;
const MAP_ROW_MAX_INDEX = 31;
const MAP_COLUMN_MAX_INDEX = 61;

/**
 * cid 转 rowkey
 */
export function cidToRowkey(cid) {
  if (!cid) {
    return null;
  }
  if (!/^\d+$/.test(cid)) {
    return null;
  }
  let cidLong = Long.fromString(cid);
  if (!checkCID(cid)) {
    return null;
  }
  const srcAddrNum = cidLong.and((1 << 11) - 1).toNumber();
  cidLong = cidLong.shiftRightUnsigned(11);
  const randomNum = cidLong.and((1 << 10) - 1).toNumber();
  cidLong = cidLong.shiftRightUnsigned(10);
  let oneLong = new Long(1);
  oneLong = oneLong.shiftLeft(32);
  oneLong = oneLong.sub(1);
  const timestamp = cidLong.and(oneLong).toNumber();
  cidLong = cidLong.shiftRightUnsigned(32);
  const regionId = cidLong.toNumber();
  const srcAddr = mappingIndexToStr(srcAddrNum);
  const rowkey = `${fixNum(3, regionId)}${fixNum(8, timestamp.toString(16))}${fixNum(3, randomNum)}${srcAddr}`;
  return rowkey;
}

/**
 *
 */
function fixNum(num, target) {
  if (String(target).length < num) {
    return repeatStr('0', num - String(target).length) + target;
  }
  return target;
}

/**
 *
 */
function repeatStr(str, times) {
  let target = '';
  for (let i = 0; i < times; i++) {
    target += str;
  }
  return target;
}

/**
 * 转 String
 */
function mappingIndexToStr(srcAddrNum) {
  const rowIndex = srcAddrNum >> 6;
  const columnIndex = srcAddrNum & (MAP_COLUMN_SIZE - 1);
  if (rowIndex > MAP_ROW_MAX_INDEX || columnIndex > MAP_COLUMN_MAX_INDEX) {
    return null;
  }
  const firstSrc = getRowRune(rowIndex);
  const secondSrc = getColumnRune(columnIndex);
  const resultStr = firstSrc + secondSrc;
  return resultStr;
}

/**
 *
 */
function getRowRune(index) {
  if (index <= 9) {
    return String.fromCharCode(index + 48);
  }

  if (index <= 20) {
    return String.fromCharCode(index + 55);
  }
  return String.fromCharCode(index + 76);
}

/**
 *
 */
function getColumnRune(index) {
  if (index <= 9) return String.fromCharCode(index + 48);
  if (index <= 35) return String.fromCharCode(index + 55);
  return String.fromCharCode(index + 61);
}

/**
 *
 */
function checkCID(cid) {
  let cidLong = Long.fromString(cid);
  // [53-63位]
  const srcAddrNum = cidLong.and((1 << 11) - 1).toNumber();
  cidLong = cidLong.shiftRightUnsigned(11);
  // [43-52位]
  const randomNum = cidLong.and((1 << 10) - 1).toNumber();
  cidLong = cidLong.shiftRightUnsigned(10);
  cidLong = cidLong.shiftRightUnsigned(32);
  const regionId = cidLong.toNumber();
  if (!(regionId >= 0 && regionId <= 999) || !(randomNum >= 0 && randomNum <= 999)) {
    return false;
  }
  const rowIndex = srcAddrNum >>> 6;
  const columnIndex = srcAddrNum & (MAP_COLUMN_SIZE - 1);
  if (rowIndex > MAP_ROW_MAX_INDEX || columnIndex > MAP_COLUMN_MAX_INDEX) {
    return false;
  }
  return true;
}

function generateRandomLongNumber(length) {
  let result = '';
  // 确保第一个数字不是0，除非长度为1
  result += Math.floor(Math.random() * 9) + 1;

  for (let i = 1; i < length; i++) {
    result += Math.floor(Math.random() * 10);
  }

  return result;
}


export default class ModalExpo extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      visible: false,
      press: false,
      animationType: 'fade',
      immerseStatusBar: false,
      hideStatusBar: false,
      hideNavigationBar: false,
    };
    this.show = this.show.bind(this);
    this.hide = this.hide.bind(this);
    this.time = `${0}ms`;
  }

  feedback(state) {
    this.setState({
      press: state === 'in',
    });
  }

  show() {
    const start = new Date().getTime();
    for (let i = 0; i < 1000;i++) {
      const randomString = generateRandomLongNumber(19);
      const rowkey = cidToRowkey(randomString);
    }
    const end = new Date().getTime();
    this.time = `${end - start}ms`;
    this.setState({
      visible: true,
    });
  }

  hide() {
    this.setState({
      visible: false,
    });
  }


  render() {
    const { press, visible } = this.state;
    return (
      <ScrollView>
        <View style={styles.container}>
          <View
            onPressIn={() => this.feedback('in')}
            onPressOut={() => this.feedback('out')}
            onClick={this.show}
            style={[styles.buttonView, {
              borderColor: SKIN_COLOR.mainLight,
              opacity: (press ? 0.5 : 1),
            }]}
          >
            <Text style={[styles.buttonText, { color: SKIN_COLOR.mainLight }]}>点击查看执行耗时</Text>
          </View>
        </View>
        <View style={{ flexDirection: 'row', justifyContent: 'center', marginTop: 20 }}>
          <Text
            onClick={() => {
              this.setState({ animationType: 'fade' });
            }}
            style={[styles.selectionText,
              { borderColor: this.state.animationType === 'fade' ? 'red' : SKIN_COLOR.mainLight },
              { color: this.state.animationType === 'fade' ? 'red' : SKIN_COLOR.mainLight },
            ]}
          >fade</Text>
          <Text
            onClick={() => {
              this.setState({ animationType: 'slide' });
            }}
            style={[styles.selectionText,
              { borderColor: this.state.animationType === 'slide' ? 'red' : SKIN_COLOR.mainLight },
              { color: this.state.animationType === 'slide' ? 'red' : SKIN_COLOR.mainLight },
            ]}
          >slide</Text>
          <Text
            onClick={() => {
              this.setState({ animationType: 'slide_fade' });
            }}
            style={[styles.selectionText,
              { borderColor: this.state.animationType === 'slide_fade' ? 'red' : SKIN_COLOR.mainLight },
              { color: this.state.animationType === 'slide_fade' ? 'red' : SKIN_COLOR.mainLight },
            ]}
          >slide_fade</Text>
        </View>
        <View style={{ flexDirection: 'row', justifyContent: 'center', marginTop: 20 }}>
          <Text
            onClick={() => {
              this.setState({ hideStatusBar: !this.state.hideStatusBar });
            }}
            style={[styles.selectionText,
              { borderColor: this.state.hideStatusBar ? 'red' : SKIN_COLOR.mainLight },
              { color: this.state.hideStatusBar ? 'red' : SKIN_COLOR.mainLight },
            ]}
          >autoHideStatusBar</Text>
        </View>
        <View style={{ flexDirection: 'row', justifyContent: 'center', marginTop: 20 }}>
          <Text
            onClick={() => {
              this.setState({ immerseStatusBar: !this.state.immerseStatusBar });
            }}
            style={[styles.selectionText,
              { borderColor: this.state.immerseStatusBar ? 'red' : SKIN_COLOR.mainLight },
              { color: this.state.immerseStatusBar ? 'red' : SKIN_COLOR.mainLight },
            ]}
          >immersionStatusBar</Text>
        </View>
        <View style={{ flexDirection: 'row', justifyContent: 'center', marginTop: 20 }}>
          <Text
            onClick={() => {
              this.setState({ hideNavigationBar: !this.state.hideNavigationBar });
            }}
            style={[styles.selectionText,
              { borderColor: this.state.hideNavigationBar ? 'red' : SKIN_COLOR.mainLight },
              { color: this.state.hideNavigationBar ? 'red' : SKIN_COLOR.mainLight },
            ]}
          >autoHideNavigationBar</Text>
        </View>
        <Modal
          transparent={true}
          animationType={this.state.animationType}
          visible={visible}
          onShow={() => {
            console.log('modal show');
          }}
          requestClose={() => { /* Trigger when hardware back pressed */ }}
          orientationChange={(evt) => {
            console.log('orientation changed', evt.orientation);
          }}
          supportedOrientations={['portrait']}
          immersionStatusBar={this.state.immerseStatusBar}
          autoHideStatusBar={this.state.hideStatusBar}
          autoHideNavigationBar={this.state.hideNavigationBar}
        >
          <View style={{ flex: 1, flexDirection: 'row', justifyContent: 'center',  backgroundColor: '#4c9afa88' }}>
            <View
              onClick={this.hide}
              style={{
                width: 200,
                height: 200,
                backgroundColor: SKIN_COLOR.otherLight,
                marginTop: 300,
                flexDirection: 'row',
                justifyContent: 'center',
              }}
            >
              <Text style={{ color: SKIN_COLOR.textWhite, fontSize: 22, marginTop: 80 }}>
                执行耗时 {this.time}
              </Text>
            </View>
          </View>
        </Modal>
      </ScrollView>
    );
  }
}


