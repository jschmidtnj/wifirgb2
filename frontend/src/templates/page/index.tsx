import React from 'react'
import './style.scss'

const Page = ({ data }: { data: Record<string, any> }) => (
  <div dangerouslySetInnerHTML={{ __html: data.post.html }} />
)
export default Page
