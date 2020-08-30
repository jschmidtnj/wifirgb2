import { Link } from 'gatsby'
import Img from 'gatsby-image'
import React from 'react'

import get from 'lodash/get'
import map from 'lodash/map'

import './style.scss'

const Post = ({ data, options }: Record<string, any>) => {
  const {
    category,
    tags,
    description,
    title,
    path,
    date,
    image,
  } = data.frontmatter
  const { isIndex } = options
  const html = get(data, 'html')
  const isMore = isIndex && !!html.match('<!--more-->')
  const fluid = get(image, 'childImageSharp.fluid')

  return (
    <div className="article" key={path}>
      <div className="container">
        <div className="info">
          <Link style={{ boxShadow: 'none' }} to={path}>
            <h1>{title}</h1>
            <time dateTime={date}>{date}</time>
          </Link>
          {Badges({ items: [category], primary: true })}
          {Badges({ items: tags })}
        </div>
        <div className="content">
          <p>{description}</p>
          {fluid ? (
            <Img
              fluid={fluid}
              style={{
                display: 'block',
                margin: '0 auto',
                maxWidth: '15rem',
              }}
            />
          ) : (
            ''
          )}
        </div>
        <div
          className="content"
          dangerouslySetInnerHTML={{
            __html: isMore ? getDescription(html) : html,
          }}
        />
        {isMore ? Button({ path, label: 'MORE', primary: true }) : ''}
      </div>
    </div>
  )
}

export default Post

const getDescription = (body: string) => {
  body = body.replace(/<blockquote>/g, '<blockquote class="blockquote">')
  if (body.match('<!--more-->')) {
    const bodySplit = body.split('<!--more-->')
    if (typeof bodySplit[0] !== 'undefined') {
      return bodySplit[0]
    }
  }
  return body
}

const Button = ({ path, label, primary }: Record<string, any>) => (
  <Link className="readmore" to={path}>
    <span
      className={`btn btn-outline-primary btn-block ${
        primary ? 'btn-outline-primary' : 'btn-outline-secondary'
      }`}
    >
      {label}
    </span>
  </Link>
)

const Badges = ({ items, primary }: Record<string, any>) =>
  map(items, (item, i) => {
    return (
      <span
        className={`badge ${primary ? 'badge-primary' : 'badge-secondary'}`}
        key={i}
      >
        {item}
      </span>
    )
  })
