// Bounded sequential traversal of real direct-child endpoints; never invent a label or ID.
export async function catalogPath(target, fetchChildren) {
  const roots = (await fetchChildren()).items.map((item) => ({ ...item }))
  const queue = [...roots], seen = new Set()
  for (let calls = 1; queue.length && calls <= 200; calls++) {
    const node = queue.shift()
    if (seen.has(node.id)) continue
    seen.add(node.id)
    if (node.id === target) return roots
    node.children = (await fetchChildren(node.id)).items.filter((item) => !seen.has(item.id)).map((item) => ({ ...item }))
    node.leaf = !node.children.length
    queue.push(...node.children)
  }
  throw new Error('未能从真实有效目录中解析原选项，请重新加载或重新选择有效选项')
}
export function catalogContains(nodes, id) {
  return nodes.some((node) => node.id === id || catalogContains(node.children || [], id))
}
