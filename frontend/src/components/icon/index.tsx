import { FontAwesomeIcon } from '@fortawesome/react-fontawesome'
import { library, IconName } from '@fortawesome/fontawesome-svg-core'
import React from 'react'

import { faGithub } from '@fortawesome/free-brands-svg-icons'
import './style.scss'

library.add(faGithub)

interface IconProps {
  name: IconName
}

const Icon = ({ name }: IconProps) => (
  <div className="icon" title={name}>
    <FontAwesomeIcon icon={['fab', name]} />
  </div>
)

export default Icon
