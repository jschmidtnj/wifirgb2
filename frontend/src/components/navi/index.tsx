import React from 'react'
import { Link } from 'gatsby'

interface NavProps {
  title: string
}

const Navigation = ({ title }: NavProps) => {
  return (
    <nav className="navbar navbar-expand navbar-dark flex-column flex-md-row bg-primary">
      <div className="container">
        <Link className="text-center" to="/">
          <h1 className="navbar-brand mb-0">{title}</h1>
        </Link>
        <div className="navbar-nav-scroll">
          <ul className="navbar-nav bd-navbar-nav flex-row"></ul>
        </div>
        <div className="navbar-nav flex-row ml-md-auto d-none d-md-flex" />
      </div>
    </nav>
  )
}

export default Navigation
