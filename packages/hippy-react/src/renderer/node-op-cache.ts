import * as UIManagerModule from '../modules/ui-manager-module';
import { Platform } from '../types';
import { getRootViewId } from '../utils/node';
import { Device } from '../global';
import { BatchChunk, NodeOperateType } from './render';


export const nodeOps: BatchChunk[] = [];

export const clearNodeOps = () => {
  nodeOps.splice(0, nodeOps.length);
};

export const getNodeOps = () => nodeOps;

export const setNodeOps = (chunks: BatchChunk[]) => {
  const rootViewId = getRootViewId();
  UIManagerModule.startBatch();
  chunks.forEach((chunk) => {
    switch (chunk.type) {
      case NodeOperateType.CREATE:
        UIManagerModule.createNode(rootViewId, chunk.nodes);
        break;
      case NodeOperateType.UPDATE:
        if (__PLATFORM__ === Platform.ios || Device.platform.OS === Platform.ios) {
          chunk.nodes.forEach(node => (
            UIManagerModule.updateNode(rootViewId, [node])
          ));
        } else {
          UIManagerModule.updateNode(rootViewId, chunk.nodes);
        }
        break;
      case NodeOperateType.DELETE:
        if (__PLATFORM__ === Platform.ios || Device.platform.OS === Platform.ios) {
          chunk.nodes.forEach(node => (
            UIManagerModule.deleteNode(rootViewId, [node])
          ));
        } else {
          UIManagerModule.deleteNode(rootViewId, chunk.nodes);
        }
        break;
      default:
        // pass
    }
  });
  UIManagerModule.endBatch();
};

export const addNodeOps = (chunk: BatchChunk[]) => {
  nodeOps.push(...chunk);
};


