import React, { ReactNode } from 'react'

import Navi from 'components/navi'
import Footer from 'components/footer'

import 'modern-normalize/modern-normalize.css'
import 'prismjs/themes/prism.css'
import 'scss/main.scss'
import 'animate.css/animate.css'
import 'font-awesome/css/font-awesome.css'
import { useStaticQuery, graphql } from 'gatsby'

interface PropsType {
  children: ReactNode
}

const Layout = (props: PropsType) => {
  const { site } = useStaticQuery(
    graphql`
      query {
        site {
          siteMetadata {
            title
          }
        }
      }
    `
  )
  return (
    <div>
      <Navi title={site.siteMetadata.title} />
      {props.children}
      <Footer />
    </div>
  )
}

export default Layout
