import React from 'react'
import Helmet from 'react-helmet'
import get from 'lodash/get'
import { useStaticQuery, graphql } from 'gatsby'

interface props {
  title: string
}

const Meta = ({ title }: props) => {
  const { site } = useStaticQuery(
    graphql`
      query {
        site {
          siteMetadata {
            title
            description
          }
        }
      }
    `
  )
  const siteTitle = get(site, 'title')
  title = title ? `${title} | ${siteTitle}` : siteTitle
  return (
    <Helmet
      title={title}
      meta={[
        { property: 'og:title', content: title },
        { property: 'og:type', content: 'website' },
        {
          property: 'og:description',
          content: get(site, 'description'),
        },
      ]}
    />
  )
}
export default Meta
