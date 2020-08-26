import { graphql } from 'gatsby'
import React from 'react'
import get from 'lodash/get'

import Post from 'templates/post'
import Meta from 'components/meta'
import Layout from 'components/layout'
import Controller from 'components/controller'

const Index = ({ data }: Record<string, any>) => {
  const posts = get(data, 'remark.posts')
  return (
    <Layout>
      <>
        <Meta title={''} />
        <Controller />
        {posts.map(({ post }: Record<string, unknown>, i: number) => (
          <Post
            data={post}
            options={{
              isIndex: true,
            }}
            key={i}
          />
        ))}
      </>
    </Layout>
  )
}

export default Index

export const pageQuery = graphql`
  query IndexQuery {
    site {
      meta: siteMetadata {
        title
        description
        author
      }
    }
    remark: allMarkdownRemark(
      sort: { fields: [frontmatter___date], order: DESC }
    ) {
      posts: edges {
        post: node {
          html
          frontmatter {
            layout
            title
            path
            category
            tags
            description
            date(formatString: "YYYY/MM/DD")
            image {
              childImageSharp {
                fluid(maxWidth: 500) {
                  ...GatsbyImageSharpFluid
                }
              }
            }
          }
        }
      }
    }
  }
`
