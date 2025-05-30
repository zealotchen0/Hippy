import React from 'react';
import {
  ScrollView,
} from '@hippy/react';


import Markdown from '../../markdown/index';

const copy1 = `# h1 Heading 8-)

**This is some bold text!**

This is normal text

> [Github 地址](https://github.com/Tencent/cherry-markdown){target="_blank"}

- [full model](index.html){target="_blank"}
- [basic](basic.html){target="_blank"}
- [H5](h5.html){target="_blank"}
- [多实例](multiple.html){target="_blank"}
- [无 toolbar](notoolbar.html){target="_blank"}
- [纯预览模式](preview_only.html){target=_blank}
- [注入](xss.html){target=_blank}
- [API](api.html){target=_blank}
- [图片所见即所得编辑尺寸](img.html){target=_blank}
- [表格所见即所得编辑尺寸](table.html){target=_blank}
- [标题自动序号](head_num.html){target=_blank}
- [流式输入模式（AI chart场景）](ai_chat.html){target=_blank}
- [VIM 编辑模式](vim.html){target=_blank}
- [应用mermaid version 10版本以上](mermaid.html){target=_blank}

Python 示例：
\`\`\`python
@requires_authorization
def somefunc(param1='', param2=0):
    '''A docstring'''
    if param1 > param2: # interesting
        print 'Greater'
    return (param2 - param1 + 1) or None

class SomeClass:
    pass

>>> message = '''interpreter
... prompt'''
\`\`\`
`;

const copy = 'This is normal text';
const copy2 = '\n根据2025年权威信息源综合分析，以下生肖在2025年桃花运表现尤为突出，主要得益于个人魅力、社交能力及运势加持：\n\n\n1. **属鼠**  \n\n   - **特点** ：社交能力强，善于沟通，风趣幽默，吸引力大。   - **运势** ：通过朋友引荐或社交活动易遇心仪对象，已婚者感情更稳固，事业与爱情双丰收。\n\n\n2. **属龙**  \n\n   - **特点** ：魅力四射，自信独立，情感表达能力强。   - **运势** ：单身者易通过聚会或兴趣班结识对象，已婚者感情加深，月老牵线助力稳定关系。\n\n\n3. **属马**  \n\n   - **特点** ：乐观开朗，幽默风趣，社交场合活跃。   - **运势** ：因积极态度吸引追求者，旅行或聚会中易发现缘分，婚恋关系更和谐。\n\n\n4. **属猴**  \n\n   - **特点** ：聪明机智，创造力强，富有吸引力。   - **运势** ：工作或生活中易引起他人注意，2025年桃花运显著提升1。\n\n\n5. **属鸡**  \n\n   - **特点** ：事业有成，幽默浪漫，选择精准。   - **运势** ：桃花星高照，年龄、出身不是障碍，月老牵线促成稳定恋情2。\n\n\n **补充说明** ：属蛇、属兔、属虎等生肖虽未在核心推荐中，但整体运势也较稳定，可通过提升社交互动或保持积极心态增强吸引力。\n';
export default class MarkdownExpo extends React.Component {
  render() {
    return (
        <ScrollView
        style={{ height: '100%' }}
      >
        <Markdown>
          {copy2}
        </Markdown>
      </ScrollView>
    );
  }
}
