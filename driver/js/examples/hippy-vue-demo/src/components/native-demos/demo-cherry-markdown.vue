<template>
  <div class="cherry-markdown-container">
    <text class="title">Cherry Markdown 编辑器</text>
    <div class="editor-container">
      <textarea
        ref="editor"
        v-model="markdownContent"
        class="markdown-editor"
        @change="handleContentChange"
      ></textarea>
    </div>
    <div class="preview-container">
      <div v-html="htmlContent" class="markdown-preview"></div>
    </div>
  </div>
</template>

<script>
import { createElement } from '@vue/runtime-core';
import Cherry from 'cherry-markdown';

export default {
  data() {
    return {
      markdownContent: '# Cherry Markdown\n\n这是一个强大的 Markdown 编辑器',
      htmlContent: '',
      cherry: null
    };
  },
  mounted() {
    // 在实际应用中，您可能需要根据 Cherry Markdown 的 API 进行适当调整
    this.initCherryMarkdown();
  },
  methods: {
    initCherryMarkdown() {
      // 由于 Hippy 环境限制，这里只是模拟 Cherry Markdown 的基本功能
      // 实际实现可能需要根据 Hippy 的能力进行调整
      this.renderMarkdown();
    },
    handleContentChange() {
      this.renderMarkdown();
    },
    renderMarkdown() {
      // 简单模拟 Markdown 到 HTML 的转换
      // 在实际应用中，您应该使用 Cherry Markdown 的 API
      const content = this.markdownContent;
      let html = content
        .replace(/# (.*?)$/gm, '<h1>$1</h1>')
        .replace(/## (.*?)$/gm, '<h2>$1</h2>')
        .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
        .replace(/\*(.*?)\*/g, '<em>$1</em>')
        .replace(/\n/g, '<br>');
      
      this.htmlContent = html;
    }
  }
};
</script>

<style scoped>
.cherry-markdown-container {
  flex: 1;
  padding: 16px;
}

.title {
  font-size: 20px;
  font-weight: bold;
  margin-bottom: 16px;
  color: #333;
}

.editor-container {
  border: 1px solid #ddd;
  border-radius: 4px;
  padding: 8px;
  margin-bottom: 16px;
  min-height: 150px;
}

.markdown-editor {
  width: 100%;
  min-height: 150px;
  font-size: 16px;
}

.preview-container {
  border: 1px solid #ddd;
  border-radius: 4px;
  padding: 16px;
  background-color: #f9f9f9;
  min-height: 150px;
}

.markdown-preview {
  font-size: 16px;
}
</style> 