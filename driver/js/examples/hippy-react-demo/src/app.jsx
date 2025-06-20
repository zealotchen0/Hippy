import React, { Component } from 'react';
import {
  ConsoleModule,
} from '@hippy/react';
import Long from 'long';
import HomeEntry from './pages/entry';
import ContainerView from './shared/ContainerView';

export default class App extends Component {
  componentDidMount() {
    ConsoleModule.log('~~~~~~~~~~~~~~~~~ This is a log from ConsoleModule ~~~~~~~~~~~~~~~~~');
    ConsoleModule.log('arm/x86 perf test start');
    for (let i = 0; i < 1000;i++) {
      const randomString = generateRandomLongNumber(19);
      const rowkey = cidToRowkey(randomString);
    }
    ConsoleModule.log('arm/x86 perf test end');
  }

  render() {
    return (
      <ContainerView>
        <HomeEntry />
      </ContainerView>
    );
  }
}

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


